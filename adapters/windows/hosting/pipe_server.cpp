#include "adapters/windows/hosting/pipe_server.hpp"
#include "adapters/windows/hosting/handle.hpp"
#include <array>
#include <optional>
#include <sddl.h>
#include <string>

namespace manager::adapters::windows {
namespace {
struct PipeIo {
    HANDLE pipe;
    HANDLE event;
};
bool finish_io(HANDLE pipe, OVERLAPPED &operation, BOOL started, DWORD *received = nullptr) {
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
    const bool complete = GetOverlappedResult(pipe, &operation, &transferred, TRUE) != FALSE;
    if (received) {
        *received = transferred;
    }
    return complete;
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

std::optional<std::string> read_message(const PipeIo &io) {
    std::array<char, 512> bytes{};
    OVERLAPPED read{};
    read.hEvent = io.event;
    DWORD count{};
    ResetEvent(io.event);
    const auto started = ReadFile(io.pipe, bytes.data(), static_cast<DWORD>(bytes.size()), &count, &read);
    if (!finish_io(io.pipe, read, started, &count) || count == 0) {
        return {};
    }
    return std::string(bytes.data(), count);
}
void write_message(const PipeIo &io, const std::string &reply) {
    OVERLAPPED write{};
    write.hEvent = io.event;
    DWORD count{};
    ResetEvent(io.event);
    finish_io(io.pipe, write,
              WriteFile(io.pipe, reply.data(), static_cast<DWORD>(reply.size()), &count, &write));
}

void answer(const PipeIo &io, const PipeHandler &handler) {
    const auto request = read_message(io);
    if (!request) {
        return;
    }
    const auto reply = handler(io.pipe, *request);
    if (reply.empty() || reply.size() > 512) {
        return;
    }
    write_message(io, reply);
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
        PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_REJECT_REMOTE_CLIENTS, 1, 512, 512, 1000, &security);
    LocalFree(descriptor);
    return pipe;
}
} // namespace

int serve_requests(HANDLE stop, const wchar_t *name, const PipeHandler &handler) {
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
            answer(io, handler);
            DisconnectNamedPipe(pipe.get());
        }
    }
    return 0;
}

int serve_diagnostics(HANDLE stop, const wchar_t *name) {
    return serve_requests(stop, name, [](HANDLE, std::string_view request) -> std::string {
        if (!request.empty() && request.back() == '\0') {
            request.remove_suffix(1);
        }
        if (request != "RMB/1 PING") {
            return {};
        }
        return std::string("RMB/1 PONG ") + MANAGER_APP_VERSION + " " + MANAGER_BUILD_ID + " " +
               std::to_string(GetCurrentProcessId());
    });
}
} // namespace manager::adapters::windows
