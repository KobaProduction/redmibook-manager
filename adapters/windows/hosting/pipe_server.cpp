#include "adapters/windows/hosting/pipe_server.hpp"
#include "adapters/windows/hosting/handle.hpp"
#include <array>
#include <sddl.h>
#include <string>

namespace manager::adapters::windows {
namespace {
struct PipeIo {
    HANDLE pipe;
    HANDLE event;
};
bool finish_io(HANDLE pipe, OVERLAPPED &operation, BOOL started) {
    if (started) {
        return true;
    }
    if (GetLastError() != ERROR_IO_PENDING) {
        return false;
    }
    if (WaitForSingleObject(operation.hEvent, 1000) != WAIT_OBJECT_0) {
        CancelIoEx(pipe, &operation);
    }
    DWORD transferred = 0;
    return GetOverlappedResult(pipe, &operation, &transferred, TRUE) != FALSE;
}

bool connect_client(const PipeIo &io, HANDLE stop, OVERLAPPED &operation) {
    if (ConnectNamedPipe(io.pipe, &operation)) {
        return true;
    }
    const auto error = GetLastError();
    if (error == ERROR_PIPE_CONNECTED) {
        return true;
    }
    if (error != ERROR_IO_PENDING) {
        return false;
    }
    const std::array<HANDLE, 2> waits{stop, operation.hEvent};
    if (WaitForMultipleObjects(2, waits.data(), FALSE, INFINITE) == WAIT_OBJECT_0 + 1) {
        return true;
    }
    CancelIoEx(io.pipe, &operation);
    DWORD transferred = 0;
    GetOverlappedResult(io.pipe, &operation, &transferred, TRUE);
    return false;
}

void await_client_close(const PipeIo &io) {
    OVERLAPPED read{};
    read.hEvent = io.event;
    DWORD count = 0;
    char extra = 0;
    ResetEvent(io.event);
    finish_io(io.pipe, read, ReadFile(io.pipe, &extra, 1, &count, &read));
}

void answer(const PipeIo &io) {
    std::array<char, 64> request{};
    OVERLAPPED read{};
    read.hEvent = io.event;
    DWORD count = 0;
    ResetEvent(io.event);
    const auto started =
        ReadFile(io.pipe, request.data(), static_cast<DWORD>(request.size() - 1), &count, &read);
    if (!finish_io(io.pipe, read, started) || std::string(request.data()) != "RMB/1 PING") {
        return;
    }
    const std::string reply = std::string("RMB/1 PONG ") + MANAGER_APP_VERSION + " " + MANAGER_BUILD_ID +
                              " " + std::to_string(GetCurrentProcessId());
    OVERLAPPED write{};
    write.hEvent = io.event;
    ResetEvent(io.event);
    finish_io(io.pipe, write,
              WriteFile(io.pipe, reply.data(), static_cast<DWORD>(reply.size()), &count, &write));
    await_client_close(io);
}

HANDLE create_pipe(const wchar_t *name) {
    PSECURITY_DESCRIPTOR descriptor = nullptr;
    if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(
            L"D:P(A;;GA;;;SY)(A;;GA;;;BA)(A;;GA;;;LS)(A;;GRGW;;;IU)", SDDL_REVISION_1, &descriptor,
            nullptr)) {
        return INVALID_HANDLE_VALUE;
    }
    SECURITY_ATTRIBUTES security{sizeof(SECURITY_ATTRIBUTES), descriptor, FALSE};
    const auto pipe = CreateNamedPipeW(
        name, PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED | FILE_FLAG_FIRST_PIPE_INSTANCE,
        PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_REJECT_REMOTE_CLIENTS, 1, 256, 64, 1000, &security);
    LocalFree(descriptor);
    return pipe;
}
} // namespace

int serve_diagnostics(HANDLE stop, const wchar_t *name) {
    const Handle pipe(create_pipe(name));
    const Handle event(CreateEventW(nullptr, TRUE, FALSE, nullptr));
    if (!pipe.valid() || !event.valid()) {
        return 1;
    }
    const PipeIo io{pipe.get(), event.get()};
    while (WaitForSingleObject(stop, 0) == WAIT_TIMEOUT) {
        OVERLAPPED connection{};
        connection.hEvent = event.get();
        ResetEvent(event.get());
        if (connect_client(io, stop, connection)) {
            answer(io);
            DisconnectNamedPipe(pipe.get());
        }
    }
    return 0;
}
} // namespace manager::adapters::windows
