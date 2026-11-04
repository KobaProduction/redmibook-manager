#pragma once
#include "application/shortcuts/input_port.hpp"
#include <array>
#include <cstddef>

namespace manager::application::shortcuts {
// Normalized held-key state. Non-letter keys (Tab, arrows, function keys, etc.)
// remain explicit blockers rather than disappearing from a modifier-only mask.
struct HeldKeys {
    std::array<bool, 26> letters{};
    bool control{};
    bool alt{};
    bool shift{};
    bool super{};
    bool other{};
    bool valid{true};
};
inline bool exact_chord(const KeyChord &required, const HeldKeys &held, bool primary_reported = false) {
    if (!held.valid || held.other || held.super || required.control != held.control ||
        required.alt != held.alt || required.shift != held.shift)
        return false;
    const bool letter = required.latin_key >= 'A' && required.latin_key <= 'Z';
    const auto index = letter ? static_cast<std::size_t>(required.latin_key - 'A') : held.letters.size();
    for (std::size_t key = 0; key < held.letters.size(); ++key)
        if (held.letters[key] && key != index)
            return false;
    return primary_reported || (letter && held.letters[index]);
}
} // namespace manager::application::shortcuts
