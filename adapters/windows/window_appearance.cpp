#include "adapters/platform/window_appearance.hpp"
#include <bit>
#include <dwmapi.h>
#include <windows.h>
namespace manager::adapters {
bool request_window_foreground(std::uintptr_t handle) {
    const auto window = std::bit_cast<HWND>(handle);
    DWORD owner{};
    GetWindowThreadProcessId(window, &owner);
    if (owner != GetCurrentProcessId())
        return false;
    BringWindowToTop(window);
    return SetForegroundWindow(window) != FALSE && GetForegroundWindow() == window;
}
bool set_window_rounding(std::uintptr_t handle, bool rounded) {
    constexpr DWORD attribute = 33;
    const DWORD preference = rounded ? 2U : 1U;
    return SUCCEEDED(
        DwmSetWindowAttribute(std::bit_cast<HWND>(handle), attribute, &preference, sizeof(preference)));
}
} // namespace manager::adapters
