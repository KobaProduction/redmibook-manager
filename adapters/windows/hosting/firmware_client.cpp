#include "adapters/platform/firmware_controls.hpp"
#include "adapters/windows/hosting/client_pipe.hpp"
#include "adapters/windows/hosting/development_host.hpp"
#include "adapters/windows/hosting/firmware_status.hpp"
#include <array>
#include <utility>
namespace manager::adapters {
namespace {
namespace wire = contracts::firmware;
namespace battery = application::battery;
namespace performance = application::performance;
HANDLE open_firmware_pipe(const wchar_t *name) {
    const auto deadline = GetTickCount64() + 5000;
    for (;;) {
        const auto now = GetTickCount64();
        if (now >= deadline)
            return INVALID_HANDLE_VALUE;
        // Wait for server availability, without fixed-delay retries or resending commands.
        if (!WaitNamedPipeW(name, static_cast<DWORD>(deadline - now)))
            return INVALID_HANDLE_VALUE;
        const auto pipe =
            CreateFileW(name, GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING,
                        FILE_FLAG_OVERLAPPED | SECURITY_SQOS_PRESENT | SECURITY_IDENTIFICATION, nullptr);
        if (pipe != INVALID_HANDLE_VALUE)
            return pipe;
        if (GetLastError() != ERROR_PIPE_BUSY)
            return INVALID_HANDLE_VALUE;
    }
}

HANDLE connect_firmware(DWORD &pid) {
    if (!development_scope().empty())
        return INVALID_HANDLE_VALUE;
    const auto name = std::wstring(windows::pipe_name) + L".Firmware";
    const auto pipe = open_firmware_pipe(name.c_str());
    if (pipe == INVALID_HANDLE_VALUE)
        return pipe;
    DWORD mode = PIPE_READMODE_MESSAGE;
    if (!windows::trusted_machine_pipe(pipe, pid) ||
        !SetNamedPipeHandleState(pipe, &mode, nullptr, nullptr)) {
        CloseHandle(pipe);
        return INVALID_HANDLE_VALUE;
    }
    return pipe;
}
bool complete_transaction(HANDLE pipe, OVERLAPPED &operation, DWORD &count) {
    if (GetLastError() != ERROR_IO_PENDING)
        return false;
    if (WaitForSingleObject(operation.hEvent, 10000) != WAIT_OBJECT_0)
        CancelIoEx(pipe, &operation);
    return GetOverlappedResult(pipe, &operation, &count, TRUE) != FALSE;
}

std::optional<std::string> transact(HANDLE pipe, const std::string &request) {
    const windows::Handle event(CreateEventW(nullptr, TRUE, FALSE, nullptr));
    if (!event.valid())
        return {};
    OVERLAPPED operation{};
    operation.hEvent = event.get();
    std::array<char, 512> bytes{};
    DWORD count{};
    const bool immediate =
        TransactNamedPipe(pipe, const_cast<char *>(request.data()), static_cast<DWORD>(request.size()),
                          bytes.data(), static_cast<DWORD>(bytes.size()), &count, &operation) != FALSE;
    if (!immediate && !complete_transaction(pipe, operation, count))
        return {};
    return std::string(bytes.data(), count);
}
wire::Reply validate_reply(wire::Command command, DWORD pid, const std::optional<std::string> &payload) {
    wire::Reply failure;
    failure.command = command;
    const auto decoded = payload ? wire::decode_reply(*payload) : std::nullopt;
    if (!decoded || decoded->command != command || decoded->process != pid) {
        failure.status = wire::mutation(command) ? wire::Status::OutcomeUnknown : wire::Status::Failed;
        failure.detail = "core-reply-missing-or-invalid; native-operation-not-cancelled";
        return failure;
    }
    if (decoded->version != MANAGER_APP_VERSION || decoded->build != MANAGER_BUILD_ID) {
        failure.status = wire::Status::Incompatible;
        failure.detail = "core-payload-incompatible";
        return failure;
    }
    return *decoded;
}

wire::Reply exchange_core(wire::Command command) {
    DWORD pid{};
    const windows::Handle pipe(connect_firmware(pid));
    if (!pipe.valid()) {
        wire::Reply failure;
        failure.command = command;
        failure.detail = "authenticated-core-unavailable";
        return failure;
    }
    return validate_reply(
        command, pid,
        transact(pipe.get(), wire::encode(wire::Request{command, MANAGER_APP_VERSION, MANAGER_BUILD_ID})));
}
battery::Observation charge_observation(const wire::Reply &reply) {
    std::optional<battery::ChargeLimit> limit;
    if (reply.value == 80 || reply.value == 100) {
        limit = static_cast<battery::ChargeLimit>(reply.value);
    }
    return {windows::application_status<battery::Status>(reply.status), limit, reply.detail};
}
performance::Observation profile_observation(const wire::Reply &reply) {
    std::optional<performance::Profile> profile;
    if (reply.value >= 1 && reply.value <= 4) {
        profile = static_cast<performance::Profile>(reply.value);
    }
    return {windows::application_status<performance::Status>(reply.status), profile, reply.detail};
}
class CoreCharge final : public battery::ChargeControl {
  public:
    battery::Capability capability() const override {
        const auto reply = exchange_core(wire::Command::ChargeCapability);
        std::vector<battery::ChargeLimit> limits;
        if (reply.allowed & 1U) {
            limits.push_back(battery::ChargeLimit::Protection80);
        }
        if (reply.allowed & 2U) {
            limits.push_back(battery::ChargeLimit::Full);
        }
        return {windows::application_status<battery::Status>(reply.status), std::move(limits), reply.detail};
    }
    battery::Observation read() override {
        return charge_observation(exchange_core(wire::Command::ReadCharge));
    }
    battery::Observation request(battery::ChargeLimit limit) override {
        if (!domain::battery::valid(limit)) {
            return {battery::Status::InvalidRequest, {}, "only-80-or-100"};
        }
        return charge_observation(exchange_core(limit == battery::ChargeLimit::Protection80
                                                    ? wire::Command::Charge80
                                                    : wire::Command::Charge100));
    }
};
class CorePerformance final : public performance::PerformanceControl {
  public:
    performance::Status capability() const override {
        return windows::application_status<performance::Status>(
            exchange_core(wire::Command::PerformanceCapability).status);
    }
    performance::Observation read() override {
        return profile_observation(exchange_core(wire::Command::ReadPerformance));
    }
    performance::Observation request(performance::Profile profile) override {
        switch (profile) {
        case performance::Profile::Balanced:
            return profile_observation(exchange_core(wire::Command::Balanced));
        case performance::Profile::Quiet:
            return profile_observation(exchange_core(wire::Command::Quiet));
        case performance::Profile::Turbo:
            return profile_observation(exchange_core(wire::Command::Turbo));
        case performance::Profile::FullSpeed:
            return {performance::Status::InvalidRequest, {}, "full-speed-unqualified"};
        }
        return {performance::Status::InvalidRequest, {}, "unknown-profile"};
    }
};
class CoreMicrophoneState final : public application::microphone::FirmwareStatePort {
  public:
    application::microphone::FirmwareState read() const override {
        return execute(wire::Command::ReadMicrophone);
    }
    application::microphone::FirmwareState request(bool muted) const override {
        return execute(muted ? wire::Command::MuteMicrophone : wire::Command::UnmuteMicrophone);
    }

  private:
    static application::microphone::FirmwareState execute(wire::Command command) {
        using application::microphone::FirmwareStateStatus;
        const auto reply = exchange_core(command);
        const auto status = windows::microphone_status(reply.status);
        if (status != FirmwareStateStatus::Ready)
            return {status, {}, reply.detail};
        if (reply.value != 0 && reply.value != 1)
            return {FirmwareStateStatus::Failed, {}, "core-microphone-state-invalid"};
        return {status, reply.value == 1, reply.detail};
    }
};
} // namespace
std::shared_ptr<application::microphone::FirmwareStatePort> make_core_microphone_state() {
    return std::make_shared<CoreMicrophoneState>();
}
std::shared_ptr<application::battery::ChargeControl> make_core_charge_control() {
    return std::make_shared<CoreCharge>();
}
std::shared_ptr<application::performance::PerformanceControl> make_core_performance_control() {
    return std::make_shared<CorePerformance>();
}
} // namespace manager::adapters
