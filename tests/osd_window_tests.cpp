#include "adapters/platform/window_appearance.hpp"
#include <bit>
#include <iostream>
#include <string>
#include <thread>
#include <windows.h>

namespace {
bool on_top(HWND window, const char *step) {
    if (GetTopWindow(nullptr) == window)
        return true;
    std::cerr << "Unexpected OSD z-order at " << step << '\n';
    return false;
}
bool verify_layer(HWND full, HWND osd) {
    ShowWindow(full, SW_SHOWNOACTIVATE);
    ShowWindow(osd, SW_SHOWNOACTIVATE);
    const auto foreground = GetForegroundWindow();
    manager::adapters::raise_osd_window(std::bit_cast<std::uintptr_t>(osd));
    if (!on_top(osd, "initial show"))
        return false;
    // A fullscreen application may have moved ahead since the previous OSD.
    if (!SetWindowPos(osd, full, 0, 0, 0, 0, SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE)) {
        std::cerr << "Unable to arrange covered OSD fixture: " << GetLastError() << '\n';
        return false;
    }
    if (!on_top(full, "fullscreen moved ahead"))
        return false;
    manager::adapters::raise_osd_window(std::bit_cast<std::uintptr_t>(osd));
    constexpr auto expected = WS_EX_TOPMOST | WS_EX_NOACTIVATE | WS_EX_TRANSPARENT | WS_EX_TOOLWINDOW;
    return on_top(osd, "repeated show") && GetForegroundWindow() == foreground &&
           (GetWindowLongPtrW(osd, GWL_EXSTYLE) & expected) == expected;
}
bool test_windows() {
    const auto instance = GetModuleHandleW(nullptr);
    WNDCLASSW type{};
    type.lpfnWndProc = DefWindowProcW;
    type.hInstance = instance;
    type.lpszClassName = L"RedmibookOsdLayerTest";
    if (!RegisterClassW(&type))
        return false;
    const auto full = CreateWindowExW(WS_EX_TOPMOST | WS_EX_NOACTIVATE, type.lpszClassName,
                                      L"Owned fullscreen test surface", WS_POPUP, 0, 0, 1920, 1080, nullptr,
                                      nullptr, instance, nullptr);
    const auto osd = CreateWindowExW(WS_EX_TOOLWINDOW, type.lpszClassName, L"Owned OSD test surface",
                                     WS_POPUP, 20, 20, 240, 80, nullptr, nullptr, instance, nullptr);
    const bool valid = full && osd && verify_layer(full, osd);
    if (osd)
        DestroyWindow(osd);
    if (full)
        DestroyWindow(full);
    UnregisterClassW(type.lpszClassName, instance);
    return valid;
}
} // namespace
int main() {
    const auto name = L"RedmibookOsdTest-" + std::to_wstring(GetCurrentProcessId());
    const auto desktop = CreateDesktopW(name.c_str(), nullptr, nullptr, 0, GENERIC_ALL, nullptr);
    if (!desktop)
        return 1;
    bool valid = false;
    std::thread test([&] { valid = SetThreadDesktop(desktop) && test_windows(); });
    test.join();
    CloseDesktop(desktop);
    if (!valid)
        std::cerr << "OSD did not remain non-activating and above the owned fullscreen window\n";
    return valid ? 0 : 1;
}
