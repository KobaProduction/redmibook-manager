#include "adapters/windows/hosting/firmware_server.hpp"
#include "adapters/device_profile/native_profile.hpp"
#include "adapters/windows/battery/charge_control.hpp"
#include "adapters/windows/hosting/development_host.hpp"
#include "adapters/windows/hosting/firmware_status.hpp"
#include "adapters/windows/performance/performance_control.hpp"
#include <utility>
namespace manager::adapters::windows {
namespace {
namespace wire = contracts::firmware;
namespace fw = windows_firmware;
class ObservedTransport final : public fw::MifsTransport {
  public:
    explicit ObservedTransport(bool &unknown) : unknown_(unknown) {}
    fw::MifsReply exchange(fw::MifsCommand command) override {
        auto result = native_.exchange(command);
        if (command != fw::MifsCommand::ReadCharge && command != fw::MifsCommand::ReadPerformance &&
            result.status == fw::TransportStatus::OutcomeUnknown) {
            unknown_ = true;
        }
        return result;
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
class Authority {
  public:
    std::string dispatch(HANDLE pipe, std::string_view text);

  private:
    bool unknown_write_{false};
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
                           : application::battery::apply_charge_limit(
                                 control, command == wire::Command::Charge80
                                              ? application::battery::ChargeLimit::Protection80
                                              : application::battery::ChargeLimit::Full);
    observation(reply, value);
    if (value.limit) {
        reply.value = static_cast<int>(*value.limit);
    }
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
void Authority::execute(const wire::Request &request, wire::Reply &reply) {
    if (unknown_write_ && request.command != wire::Command::ReadCharge &&
        request.command != wire::Command::ReadPerformance) {
        reply.status = wire::Status::OutcomeUnknown;
        reply.allowed = 0;
        reply.detail = "previous-native-write-unsettled; manual-reconciliation-required";
        return;
    }
    const auto profile = NativeDeviceProfile{}.read();
    if (profile.status != application::ProfileReadStatus::Available) {
        reply.status = wire::Status::Failed;
        reply.detail = "device-identity-unavailable";
        return;
    }
    const fw::FirmwareIdentity identity{profile.manufacturer, profile.board_model, profile.bios_version};
    ObservedTransport transport(unknown_write_);
    if (request.command <= wire::Command::Charge100) {
        battery(request.command, transport, identity, reply);
    } else {
        performance(request.command, transport, identity, reply);
    }
}
std::string Authority::dispatch(HANDLE pipe, std::string_view text) {
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
int serve_firmware(HANDLE stop) {
    if (!development_scope().empty()) {
        return 2;
    }
    Authority authority;
    const auto name = std::wstring(pipe_name) + L".Firmware";
    return serve_requests(stop, name.c_str(), [&authority](HANDLE pipe, std::string_view text) {
        return authority.dispatch(pipe, text);
    });
}
} // namespace manager::adapters::windows
