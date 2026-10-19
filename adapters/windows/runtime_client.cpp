#include "adapters/platform/development_scope.hpp"
#include "adapters/platform/runtime.hpp"
#include "adapters/windows/hosting/client_pipe.hpp"
#include "adapters/windows/hosting/handle.hpp"
#include <array>
#include <shellapi.h>
#include <sstream>

namespace manager::adapters {
application::DiagnosticReply NativeDiagnostic::probe() const {
    return windows::probe_daemon();
}

std::string session_identity() {
    std::array<char, 256> user{};
    DWORD size = static_cast<DWORD>(user.size());
    DWORD session = 0;
    if (!GetUserNameA(user.data(), &size) || !ProcessIdToSessionId(GetCurrentProcessId(), &session)) {
        return {};
    }
    return std::string(user.data()) + ":" + std::to_string(session);
}

void register_application_restart() {
    RegisterApplicationRestart(L"--restored", RESTART_NO_CRASH | RESTART_NO_HANG | RESTART_NO_REBOOT);
}

bool restart_machine_runtime() {
    if (!development_scope().empty()) {
        return false;
    }
    std::array<wchar_t, 32768> path{};
    if (GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size())) == 0) {
        return false;
    }
    std::wstring executable(path.data());
    executable = executable.substr(0, executable.find_last_of(L"\\/")) + L"\\redmibook-service.exe";
    SHELLEXECUTEINFOW info{};
    info.cbSize = sizeof(info);
    info.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_FLAG_NO_UI;
    info.lpVerb = L"runas";
    info.lpFile = executable.c_str();
    info.lpParameters = L"--restart";
    info.nShow = SW_HIDE;
    if (!ShellExecuteExW(&info)) {
        return false;
    }
    const windows::Handle process(info.hProcess);
    DWORD code = 1;
    return WaitForSingleObject(process.get(), 20000) == WAIT_OBJECT_0 &&
           GetExitCodeProcess(process.get(), &code) && code == 0;
}

void permit_foreground_activation(std::int64_t process_id) {
    if (process_id > 0 && process_id <= 0xffffffffLL) {
        AllowSetForegroundWindow(static_cast<DWORD>(process_id));
    }
}
} // namespace manager::adapters
