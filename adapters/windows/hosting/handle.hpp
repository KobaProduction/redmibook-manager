#pragma once

#include <windows.h>

namespace manager::adapters::windows {
class Handle final {
  public:
    explicit Handle(HANDLE value) : value_(value) {}
    ~Handle() {
        if (valid()) {
            CloseHandle(value_);
        }
    }
    Handle(const Handle &) = delete;
    Handle &operator=(const Handle &) = delete;
    [[nodiscard]] HANDLE get() const {
        return value_;
    }
    [[nodiscard]] bool valid() const {
        return value_ != nullptr && value_ != INVALID_HANDLE_VALUE;
    }

  private:
    HANDLE value_;
};

inline constexpr wchar_t service_name[] = L"RedmibookManager";
inline constexpr wchar_t pipe_name[] = L"\\\\.\\pipe\\KobaProduction.RedmibookManager.Diagnostic.v1";
inline constexpr wchar_t test_pipe_name[] =
    L"\\\\.\\pipe\\KobaProduction.RedmibookManager.Diagnostic.Test.v1";
} // namespace manager::adapters::windows
