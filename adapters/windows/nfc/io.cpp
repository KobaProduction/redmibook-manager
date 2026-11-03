#include "adapters/windows/nfc/io.hpp"
#include <array>
#include <thread>

namespace manager::adapters::nfc::windows {
namespace {
struct Request {
    std::shared_ptr<Handle> device;
    application::nfc::Bytes input;
    application::nfc::Bytes output;
    Handle event{CreateEventW(nullptr, TRUE, FALSE, nullptr)};
    OVERLAPPED overlapped{};
    explicit Request(std::shared_ptr<Handle> handle, application::nfc::Bytes bytes, std::size_t size)
        : device(std::move(handle)), input(std::move(bytes)), output(size) {
        overlapped.hEvent = event.value;
    }
};
DWORD begin(Request &request, DWORD code) {
    DWORD count{};
    const bool started =
        code == 0 ? WriteFile(request.device->value, request.input.data(),
                              static_cast<DWORD>(request.input.size()), &count, &request.overlapped) != FALSE
                  : DeviceIoControl(request.device->value, code, request.input.data(),
                                    static_cast<DWORD>(request.input.size()), request.output.data(),
                                    static_cast<DWORD>(request.output.size()), &count,
                                    &request.overlapped) != FALSE;
    return started ? ERROR_SUCCESS : GetLastError();
}
void retain_cancelled(std::shared_ptr<Request> request, std::shared_ptr<std::atomic<bool>> pending) {
    CancelIoEx(request->device->value, &request->overlapped);
    // Cancellation is a request. Keep handle, OVERLAPPED and buffers alive until
    // native completion, even after our public deadline or transport destruction.
    std::thread([request = std::move(request), pending = std::move(pending)] {
        DWORD transferred{};
        GetOverlappedResult(request->device->value, &request->overlapped, &transferred, TRUE);
        *pending = false;
    }).detach();
}
DWORD wait_request(Request &request, DWORD deadline, const std::stop_token &stop) {
    if (!stop.stop_possible())
        return WaitForSingleObject(request.event.value, deadline);
    Handle interrupted{CreateEventW(nullptr, TRUE, FALSE, nullptr)};
    if (!interrupted.value)
        return WAIT_FAILED;
    const std::stop_callback wake(stop, [&interrupted] { SetEvent(interrupted.value); });
    const std::array events{request.event.value, interrupted.value};
    // The worker can stop even if the driver has not acknowledged cancellation.
    // retain_cancelled keeps native dependencies alive until actual completion.
    return WaitForMultipleObjects(static_cast<DWORD>(events.size()), events.data(), FALSE, deadline);
}
Completion completed_request(Request &request, std::size_t output_size) {
    DWORD transferred{};
    const bool completed =
        GetOverlappedResult(request.device->value, &request.overlapped, &transferred, FALSE);
    const auto error = completed ? ERROR_SUCCESS : GetLastError();
    if (completed && output_size != 0)
        request.output.resize(transferred);
    return {error, std::move(request.output)};
}
} // namespace
Completion transfer(const std::shared_ptr<Handle> &device, DWORD code, application::nfc::Bytes input,
                    std::size_t output_size, const std::shared_ptr<std::atomic<bool>> &pending,
                    DWORD deadline, const std::stop_token &stop) {
    if (pending->exchange(true))
        return {ERROR_BUSY, {}};
    auto request = std::make_shared<Request>(device, std::move(input), output_size);
    const auto error = begin(*request, code);
    if (error != ERROR_SUCCESS && error != ERROR_IO_PENDING) {
        *pending = false;
        return {error, {}};
    }
    const auto waited = wait_request(*request, deadline, stop);
    if (waited != WAIT_OBJECT_0) {
        retain_cancelled(request, pending);
        return {static_cast<DWORD>(waited == WAIT_OBJECT_0 + 1 ? ERROR_OPERATION_ABORTED : ERROR_TIMEOUT),
                {}};
    }
    auto completion = completed_request(*request, output_size);
    *pending = false;
    return completion;
}
application::nfc::Status status(DWORD error) {
    using application::nfc::Status;
    if (error == ERROR_SUCCESS)
        return Status::Ready;
    if (error == ERROR_ACCESS_DENIED)
        return Status::Denied;
    if (error == ERROR_SHARING_VIOLATION || error == ERROR_BUSY)
        return Status::Busy;
    if (error == ERROR_TIMEOUT || error == WAIT_TIMEOUT)
        return Status::TimedOut;
    if (error == ERROR_FILE_NOT_FOUND || error == ERROR_DEVICE_NOT_CONNECTED)
        return Status::Unavailable;
    return Status::Failed;
}
} // namespace manager::adapters::nfc::windows
