#pragma once

#include <cstdint>

namespace manager::application::shortcuts {

// Invalid is a decoding marker, rejected by valid_binding_action before any execution.
enum class BindingAction : std::uint8_t { Default, OpenManager, None, OpenTray, Invalid = 255 };

[[nodiscard]] constexpr bool valid_binding_action(int action) noexcept {
    return action >= 0 && action <= 3;
}

} // namespace manager::application::shortcuts
