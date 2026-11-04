#include "adapters/windows/shortcuts/key_state.hpp"
#include <vector>
#include <windows.h>

namespace manager::adapters::windows_shortcuts {
namespace {
void record(application::shortcuts::HeldKeys &held, std::uint8_t key) {
    if (key >= 'A' && key <= 'Z') {
        held.letters[static_cast<std::size_t>(key - 'A')] = true;
        return;
    }
    if (key == VK_CONTROL || key == VK_LCONTROL || key == VK_RCONTROL)
        held.control = true;
    else if (key == VK_MENU || key == VK_LMENU || key == VK_RMENU)
        held.alt = true;
    else if (key == VK_SHIFT || key == VK_LSHIFT || key == VK_RSHIFT)
        held.shift = true;
    else if (key == VK_LWIN || key == VK_RWIN)
        held.super = true;
    else
        held.other = true;
}
} // namespace
application::shortcuts::HeldKeys decode_held_keys(std::span<const std::uint8_t> keys) {
    application::shortcuts::HeldKeys held;
    for (const auto key : keys)
        record(held, key);
    return held;
}
application::shortcuts::HeldKeys capture_held_keys() {
    // Sample once on input receipt, never from a page timer or after queued dispatch.
    const auto desktop = OpenInputDesktop(0, FALSE, DESKTOP_READOBJECTS);
    if (desktop == nullptr)
        return {.valid = false};
    CloseDesktop(desktop);
    std::vector<std::uint8_t> keys;
    for (int key = 1; key < 256; ++key)
        if (GetAsyncKeyState(key) < 0)
            keys.push_back(static_cast<std::uint8_t>(key));
    return decode_held_keys(keys);
}
} // namespace manager::adapters::windows_shortcuts
