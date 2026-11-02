#include "adapters/platform/window_appearance.hpp"
#include <bit>
#include <dwmapi.h>
#include <iostream>
#include <windows.h>
namespace manager::adapters {
void raise_osd_window(std::uintptr_t handle) {
    const auto window = std::bit_cast<HWND>(handle);
    DWORD owner{};
    GetWindowThreadProcessId(window, &owner);
    if (owner != GetCurrentProcessId())
        return;
    const auto style = GetWindowLongPtrW(window, GWL_EXSTYLE);
    SetLastError(ERROR_SUCCESS);
    const auto previous = SetWindowLongPtrW(window, GWL_EXSTYLE,
                                            style | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW | WS_EX_TRANSPARENT);
    if ((previous == 0 && GetLastError() != ERROR_SUCCESS) ||
        !SetWindowPos(window, HWND_TOPMOST, 0, 0, 0, 0,
                      SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE | SWP_NOOWNERZORDER))
        std::cerr << "Unable to raise RedmiBook Manager OSD: " << GetLastError() << '\n';
}
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
