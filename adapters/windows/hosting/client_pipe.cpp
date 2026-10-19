#include "adapters/windows/hosting/client_pipe.hpp"
#include "adapters/windows/hosting/development_host.hpp"
#include "adapters/windows/hosting/handle.hpp"
#include <array>
#include <sstream>

namespace manager::adapters::windows {
namespace {
bool trusted_server(HANDLE pipe, DWORD &pid) {
    if (!GetNamedPipeServerProcessId(pipe, &pid)) {
        return false;
    }
    if (!development_scope().empty()) {
        return development_server(pid);
    }
    const auto manager = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT);
    if (manager == nullptr) {
        return false;
    }
    const auto service = OpenServiceW(manager, service_name, SERVICE_QUERY_STATUS);
    SERVICE_STATUS_PROCESS status{};
    DWORD received = 0;
    const bool valid = service != nullptr &&
                       QueryServiceStatusEx(service, SC_STATUS_PROCESS_INFO,
                                            reinterpret_cast<BYTE *>(&status), sizeof(status), &received) &&
                       status.dwProcessId == pid;
    if (service != nullptr) {
        CloseServiceHandle(service);
    }
    CloseServiceHandle(manager);
    return valid;
}

bool transact(HANDLE pipe, std::array<char, 256> &buffer, DWORD &received) {
    const Handle event(CreateEventW(nullptr, TRUE, FALSE, nullptr));
    if (!event.valid()) {
        return false;
    }
    OVERLAPPED operation{};
    operation.hEvent = event.get();
    char request[] = "RMB/1 PING";
    if (TransactNamedPipe(pipe, request, sizeof(request), buffer.data(), static_cast<DWORD>(buffer.size()),
                          &received, &operation)) {
        return true;
    }
    if (GetLastError() != ERROR_IO_PENDING) {
        return false;
    }
    if (WaitForSingleObject(event.get(), 1000) != WAIT_OBJECT_0) {
        CancelIoEx(pipe, &operation);
    }
    return GetOverlappedResult(pipe, &operation, &received, TRUE) != FALSE;
}

application::DiagnosticReply decode(std::string_view payload, DWORD pid) {
    std::istringstream response{std::string(payload)};
    std::string protocol;
    std::string verb;
    application::DiagnosticReply result;
    response >> protocol >> verb >> result.version >> result.build >> result.process_id;
    result.connected = !response.fail() && protocol == "RMB/1" && verb == "PONG" && result.process_id == pid;
    return result;
}
} // namespace

bool trusted_machine_pipe(HANDLE pipe, DWORD &pid) {
    return trusted_server(pipe, pid);
}

application::DiagnosticReply probe_daemon() {
    const auto name = diagnostic_endpoint();
    if (!WaitNamedPipeW(name.c_str(), 500)) {
        return {};
    }
    const Handle pipe(CreateFileW(name.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING,
                                  FILE_FLAG_OVERLAPPED | SECURITY_SQOS_PRESENT | SECURITY_IDENTIFICATION,
                                  nullptr));
    DWORD pid = 0;
    if (!pipe.valid() || !trusted_server(pipe.get(), pid)) {
        return {};
    }
    DWORD mode = PIPE_READMODE_MESSAGE;
    if (!SetNamedPipeHandleState(pipe.get(), &mode, nullptr, nullptr)) {
        return {};
    }
    std::array<char, 256> buffer{};
    DWORD received = 0;
    return transact(pipe.get(), buffer, received) ? decode(std::string_view(buffer.data(), received), pid)
                                                  : application::DiagnosticReply{};
}
} // namespace manager::adapters::windows
