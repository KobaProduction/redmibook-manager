#pragma once
#include "application/shortcuts/chord_match.hpp"
#include <cstdint>
#include <span>

namespace manager::adapters::windows_shortcuts {
application::shortcuts::HeldKeys decode_held_keys(std::span<const std::uint8_t> keys);
application::shortcuts::HeldKeys capture_held_keys();
} // namespace manager::adapters::windows_shortcuts
