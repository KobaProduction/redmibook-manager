#include "adapters/windows/shortcuts/key_state.hpp"
#include <array>
#include <windows.h>

namespace {
bool rejected_keys() {
    using manager::application::shortcuts::exact_chord;
    for (unsigned int key = 1; key < 256; ++key) {
        const std::array pressed{static_cast<std::uint8_t>(key)};
        const auto held = manager::adapters::windows_shortcuts::decode_held_keys(pressed);
        if (exact_chord({.latin_key = 'S'}, held, true) != (key == 'S'))
            return false;
        if (exact_chord({.latin_key = 'K'}, held, true) != (key == 'K'))
            return false;
    }
    return true;
}
bool required_modifiers() {
    const std::array<std::uint8_t, 3> pressed{VK_LCONTROL, VK_RMENU, 'M'};
    const auto held = manager::adapters::windows_shortcuts::decode_held_keys(pressed);
    return manager::application::shortcuts::exact_chord({.latin_key = 'M', .control = true, .alt = true},
                                                        held) &&
           !manager::application::shortcuts::exact_chord({.latin_key = 'M', .control = true}, held);
}
} // namespace
int main() {
    return rejected_keys() && required_modifiers() ? 0 : 1;
}
