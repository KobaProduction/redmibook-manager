#pragma once
#include "application/nfc/port.hpp"
#include <atomic>
#include <memory>
#include <stop_token>
#include <windows.h>

namespace manager::adapters::nfc::windows {
struct Handle {
    explicit Handle(HANDLE value) : value(value) {}
    ~Handle() {
        if (value && value != INVALID_HANDLE_VALUE)
            CloseHandle(value);
    }
    Handle(const Handle &) = delete;
    Handle &operator=(const Handle &) = delete;
    HANDLE value;
};
struct Completion {
    DWORD error{};
    application::nfc::Bytes bytes;
};
Completion transfer(const std::shared_ptr<Handle> &device, DWORD code, application::nfc::Bytes input,
                    std::size_t output_size, const std::shared_ptr<std::atomic<bool>> &pending,
                    DWORD deadline = 1500, const std::stop_token &stop = {});
application::nfc::Status status(DWORD error);
bool supported_driver();
bool present();
} // namespace manager::adapters::nfc::windows
