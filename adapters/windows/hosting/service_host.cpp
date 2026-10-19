#include "adapters/platform/runtime.hpp"
#include "adapters/windows/hosting/handle.hpp"
#include "adapters/windows/hosting/pipe_server.hpp"
#include "adapters/windows/hosting/service_admin.hpp"
#include <cstring>
#include <iostream>
#include <shellapi.h>
#include <string>

namespace manager::adapters {
namespace {
enum class ServicePhase : std::uint8_t {
    Running = SERVICE_RUNNING,
    Stopping = SERVICE_STOP_PENDING,
    Stopped = SERVICE_STOPPED
};
struct ServiceState {
    HANDLE stop;
    SERVICE_STATUS_HANDLE handle{nullptr};
};

void report(const ServiceState &state, ServicePhase phase, DWORD error = NO_ERROR) {
    SERVICE_STATUS value{};
    value.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    value.dwCurrentState = static_cast<DWORD>(phase);
    value.dwControlsAccepted =
        phase == ServicePhase::Running ? SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN : 0;
    value.dwWin32ExitCode = error;
    value.dwWaitHint = phase == ServicePhase::Stopping ? 3000 : 0;
    SetServiceStatus(state.handle, &value);
}

DWORD WINAPI control(DWORD code, DWORD, void *, void *context) {
    const auto &state = *static_cast<ServiceState *>(context);
    if (code == SERVICE_CONTROL_STOP || code == SERVICE_CONTROL_SHUTDOWN) {
        report(state, ServicePhase::Stopping);
        SetEvent(state.stop);
    }
    return NO_ERROR;
}

void WINAPI service_main(DWORD, wchar_t **) {
    const windows::Handle stop(CreateEventW(nullptr, TRUE, FALSE, nullptr));
    ServiceState state{stop.get()};
    state.handle = RegisterServiceCtrlHandlerExW(windows::service_name, control, &state);
    if (state.handle == nullptr) {
        return;
    }
    if (!stop.valid()) {
        report(state, ServicePhase::Stopped, ERROR_NOT_ENOUGH_MEMORY);
        return;
    }
    report(state, ServicePhase::Running);
    const int result = windows::serve_diagnostics(stop.get(), windows::pipe_name);
    report(state, ServicePhase::Stopped, result == 0 ? NO_ERROR : ERROR_SERVICE_SPECIFIC_ERROR);
}

int run_service() {
    SERVICE_TABLE_ENTRYW table[]{{const_cast<wchar_t *>(windows::service_name), service_main},
                                 {nullptr, nullptr}};
    return StartServiceCtrlDispatcherW(table) ? 0 : 1;
}

int maintenance_command() {
    int count = 0;
    auto *arguments = CommandLineToArgvW(GetCommandLineW(), &count);
    if (arguments == nullptr) {
        return 1;
    }
    const int result =
        count == 4 ? windows::maintain_installation({.directory = arguments[2], .marker = arguments[3]}) : 2;
    LocalFree(static_cast<void *>(arguments));
    return result;
}
} // namespace

int run_machine_host(int argc, char *argv[]) {
    if (argc != 2 && argc != 4) {
        return 2;
    }
    const std::string command(argv[1]);
    if (command == "--service") {
        return run_service();
    }
    if (command == "--maintain" && argc == 4) {
        return maintenance_command();
    }
    if (command == "--probe") {
        const auto result = NativeDiagnostic{}.probe();
        std::cout << result.version << ' ' << result.build << ' ' << result.process_id << '\n';
        return result.connected ? 0 : 1;
    }
    return windows::service_command(command);
}
} // namespace manager::adapters
