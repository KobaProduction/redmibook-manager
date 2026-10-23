#include "adapters/windows/hosting/firmware_server.hpp"
#include "adapters/device_profile/native_profile.hpp"
#include "adapters/platform/battery.hpp"
#include "adapters/windows/battery/charge_control.hpp"
#include "adapters/windows/battery/policy_store.hpp"
#include "adapters/windows/firmware/microphone_state.hpp"
#include "adapters/windows/hosting/development_host.hpp"
#include "adapters/windows/hosting/firmware_status.hpp"
#include "adapters/windows/performance/performance_control.hpp"
#include <array>
#include <mutex>
#include <thread>
#include <utility>
namespace manager::adapters::windows {
namespace {
namespace wire = contracts::firmware;
namespace fw = windows_firmware;
class ObservedTransport final : public fw::MifsTransport {
  public:
    explicit ObservedTransport(bool &unknown) : unknown_(unknown) {}
    fw::MifsReply exchange(fw::MifsCommand command) override {
        return fw::observe_reply(command, native_.exchange(command), unknown_);
    }

  private:
    bool &unknown_;
    fw::NativeWmiTransport native_;
};
bool console_caller(HANDLE pipe) {
    if (!ImpersonateNamedPipeClient(pipe)) {
        return false;
    }
    HANDLE raw{};
    DWORD session = 0xffffffff;
    DWORD received{};
    const bool opened = OpenThreadToken(GetCurrentThread(), TOKEN_QUERY, TRUE, &raw) != FALSE;
    const Handle token(raw);
    const bool queried =
        opened && GetTokenInformation(token.get(), TokenSessionId, &session, sizeof(session), &received);
    const bool reverted = RevertToSelf() != FALSE;
    const auto console = WTSGetActiveConsoleSessionId();
    return queried && reverted && console != 0xffffffff && session == console;
}
wire::Reply base_reply(wire::Command command) {
    wire::Reply reply;
    reply.command = command;
    reply.version = MANAGER_APP_VERSION;
    reply.build = MANAGER_BUILD_ID;
    reply.process = GetCurrentProcessId();
    return reply;
}
template <class Observation> void observation(wire::Reply &reply, const Observation &value) {
    reply.status = wire_status(value.status);
    reply.detail = value.detail;
}
void microphone(ObservedTransport &transport, const fw::FirmwareIdentity &identity, wire::Reply &reply) {
    const auto state = reply.command == wire::Command::ReadMicrophone
                           ? fw::read_microphone_state(transport, identity)
                           : fw::request_microphone_state(transport, identity,
                                                          reply.command == wire::Command::MuteMicrophone);
    observation(reply, state);
    if (state.desired_muted)
        reply.value = *state.desired_muted ? 1 : 0;
}
class Authority {
  public:
    std::string dispatch(HANDLE pipe, std::string_view text);
    void reconcile(std::string_view reason);
    void guard();

  private:
    bool unknown_write_{false};
    std::mutex mutex_;
    windows_battery::PolicyStore store_;
    windows_battery::PolicyLog audit_;
    application::battery::MaintainChargePolicy policy_{store_, &audit_};
    std::shared_ptr<application::battery::TelemetryPort> telemetry_{make_battery_telemetry()};
    std::string guard_detail_{"not-sampled"};
    ULONGLONG guard_sample_at_{};
    void apply_guard(const application::battery::Telemetry &sample);
    bool admit(wire::Command command, wire::Reply &reply);
    void execute(const wire::Request &request, wire::Reply &reply);
    void battery(wire::Command command, ObservedTransport &transport, const fw::FirmwareIdentity &identity,
                 wire::Reply &reply);
    void performance(wire::Command command, ObservedTransport &transport,
                     const fw::FirmwareIdentity &identity, wire::Reply &reply);
};
void Authority::battery(wire::Command command, ObservedTransport &transport,
                        const fw::FirmwareIdentity &identity, wire::Reply &reply) {
    windows_battery::ChargeControl control(transport, identity);
    if (command == wire::Command::ChargeCapability) {
        const auto capability = control.capability();
        reply.status = wire_status(capability.status);
        reply.allowed = capability.status == application::battery::Status::Available ? 3U : 0U;
        reply.detail = capability.detail;
        return;
    }
    const auto value = command == wire::Command::ReadCharge
                           ? control.read()
                           : policy_.select(control, command == wire::Command::Charge80
                                                         ? application::battery::ChargeLimit::Protection80
                                                         : application::battery::ChargeLimit::Full);
    observation(reply, value);
    if (value.limit) {
        reply.value = static_cast<int>(*value.limit);
    }
    if (command == wire::Command::ReadCharge && !audit_.last_record().empty())
        reply.detail += " audit=" + audit_.last_record();
    if (command == wire::Command::ReadCharge)
        reply.detail += " guard=" + guard_detail_ + ";ageMs=" +
                        std::to_string(guard_sample_at_ ? GetTickCount64() - guard_sample_at_ : 0);
}
void Authority::performance(wire::Command command, ObservedTransport &transport,
                            const fw::FirmwareIdentity &identity, wire::Reply &reply) {
    windows_performance::PerformanceControl control(transport, identity);
    if (command == wire::Command::PerformanceCapability) {
        reply.status = wire_status(control.capability());
        reply.allowed = reply.status == wire::Status::Available ? 7U : 0U;
        return;
    }
    const auto profile = command == wire::Command::Quiet   ? application::performance::Profile::Quiet
                         : command == wire::Command::Turbo ? application::performance::Profile::Turbo
                                                           : application::performance::Profile::Balanced;
    const auto value = command == wire::Command::ReadPerformance
                           ? control.read()
                           : application::performance::apply_profile(control, profile);
    observation(reply, value);
    if (value.profile) {
        reply.value = static_cast<int>(*value.profile);
    }
}
bool Authority::admit(wire::Command command, wire::Reply &reply) {
    if ((unknown_write_ || store_.load().unsettled) && command != wire::Command::ReadCharge &&
        command != wire::Command::ReadPerformance && command != wire::Command::ReadMicrophone) {
        reply.status = wire::Status::OutcomeUnknown;
        reply.allowed = 0;
        reply.detail = "previous-native-write-unsettled; manual-reconciliation-required";
        return false;
    }
    return true;
}
void Authority::execute(const wire::Request &request, wire::Reply &reply) {
    if (!admit(request.command, reply))
        return;
    const auto profile = NativeDeviceProfile{}.read();
    if (profile.status != application::ProfileReadStatus::Available) {
        reply.status = wire::Status::Failed;
        reply.detail = "device-identity-unavailable";
        return;
    }
    const fw::FirmwareIdentity identity{profile.manufacturer, profile.board_model, profile.bios_version};
    ObservedTransport transport(unknown_write_);
    if (request.command >= wire::Command::ReadMicrophone) {
        microphone(transport, identity, reply);
        unknown_write_ = unknown_write_ || reply.status == wire::Status::OutcomeUnknown;
    } else if (request.command <= wire::Command::Charge100) {
        battery(request.command, transport, identity, reply);
    } else {
        performance(request.command, transport, identity, reply);
    }
}
std::string Authority::dispatch(HANDLE pipe, std::string_view text) {
    const std::lock_guard lock(mutex_);
    const auto request = wire::decode_request(text);
    if (!request) {
        return {};
    }
    auto reply = base_reply(request->command);
    if (request->version != reply.version || request->build != reply.build) {
        reply.status = wire::Status::Incompatible;
        reply.detail = "core-payload-incompatible";
    } else if (!console_caller(pipe)) {
        reply.status = wire::Status::PermissionDenied;
        reply.detail = "active-console-session-required";
    } else {
        try {
            execute(*request, reply);
        } catch (...) {
            reply.status =
                wire::mutation(request->command) ? wire::Status::OutcomeUnknown : wire::Status::Failed;
            unknown_write_ = unknown_write_ || wire::mutation(request->command);
        }
    }
    return wire::encode(reply);
}
} // namespace
void Authority::reconcile(std::string_view reason) {
    const std::lock_guard lock(mutex_);
    if (unknown_write_)
        return;
    const auto desired = store_.load();
    if (!desired.valid || !desired.desired || desired.unsettled)
        return;
    const auto profile = NativeDeviceProfile{}.read();
    if (profile.status != application::ProfileReadStatus::Available)
        return;
    const fw::FirmwareIdentity identity{profile.manufacturer, profile.board_model, profile.bios_version};
    ObservedTransport transport(unknown_write_);
    windows_battery::ChargeControl control(transport, identity);
    try {
        const auto result = policy_.reconcile(control, reason);
        unknown_write_ = unknown_write_ || result.status == application::battery::Status::OutcomeUnknown;
        OutputDebugStringA(("Redmibook charge reconciliation: " + result.detail + "\n").c_str());
    } catch (...) {
        unknown_write_ = true;
    }
}

void Authority::guard() {
    const std::lock_guard lock(mutex_);
    const auto desired = store_.load();
    if (unknown_write_ || !desired.valid || desired.unsettled ||
        desired.desired != application::battery::ChargeLimit::Protection80)
        return;
    application::battery::Telemetry sample;
    try {
        sample = telemetry_->read();
    } catch (...) {
        guard_detail_ = "telemetry-failed";
        return;
    }
    guard_sample_at_ = GetTickCount64();
    guard_detail_ = "status=" + std::to_string(static_cast<unsigned>(sample.status)) +
                    ",percent=" + (sample.percent ? std::to_string(*sample.percent) : "unknown") +
                    ",charging=" + (sample.charging ? (*sample.charging ? "1" : "0") : "unknown") +
                    ",watts=" + (sample.rate_w ? std::to_string(*sample.rate_w) : "unknown");
    if (!application::battery::over_limit_charging(sample))
        return;
    apply_guard(sample);
}

void Authority::apply_guard(const application::battery::Telemetry &sample) {
    const auto profile = NativeDeviceProfile{}.read();
    if (profile.status != application::ProfileReadStatus::Available)
        return;
    const fw::FirmwareIdentity identity{profile.manufacturer, profile.board_model, profile.bios_version};
    ObservedTransport transport(unknown_write_);
    windows_battery::ChargeControl control(transport, identity);
    try {
        const auto result = policy_.guard(control, sample);
        unknown_write_ = unknown_write_ || result.status == application::battery::Status::OutcomeUnknown;
    } catch (...) {
        unknown_write_ = true;
    }
}

namespace {
bool arm_guard_timer(HANDLE timer) {
    constexpr LONG guard_interval_ms = 10000;
    LARGE_INTEGER due{};
    due.QuadPart = -static_cast<LONGLONG>(guard_interval_ms) * 10000;
    return SetWaitableTimer(timer, &due, guard_interval_ms, nullptr, nullptr, FALSE) != FALSE;
}
void monitor_policy(Authority &authority, HANDLE stop, HANDLE power_changed, HANDLE guard_timer) {
    const std::array<HANDLE, 3> events{stop, power_changed, guard_timer};
    bool initial = true;
    while (true) {
        const auto event = WaitForMultipleObjects(3, events.data(), FALSE, INFINITE);
        if (event == WAIT_OBJECT_0 + 1) {
            authority.reconcile(initial ? "startup" : "power-or-resume");
            if (initial)
                authority.guard();
            initial = false;
        } else if (event == WAIT_OBJECT_0 + 2) {
            authority.guard();
        } else {
            break;
        }
    }
}
} // namespace
int serve_firmware(HANDLE stop) {
    if (!development_scope().empty())
        return 2;
    Authority authority;
    const Handle power_changed(CreateEventW(nullptr, FALSE, TRUE, nullptr));
    const Handle guard_timer(CreateWaitableTimerW(nullptr, FALSE, nullptr));
    if (!power_changed.valid() || !guard_timer.valid() || !arm_guard_timer(guard_timer.get()))
        return 1;
    auto watch = watch_power([event = power_changed.get()] { SetEvent(event); }, false);
    std::jthread monitor([&] { monitor_policy(authority, stop, power_changed.get(), guard_timer.get()); });
    const auto name = std::wstring(pipe_name) + L".Firmware";
    const auto result = serve_requests(stop, name.c_str(), [&authority](HANDLE pipe, std::string_view text) {
        return authority.dispatch(pipe, text);
    });
    SetEvent(stop);
    return result;
}
} // namespace manager::adapters::windows
