#pragma once

#include <cstdint>

namespace manager::application::shortcuts {

enum class BindingAction : std::uint8_t { Default, OpenManager, None };

[[nodiscard]] constexpr bool valid_binding_action(int action) noexcept {
    return action >= 0 && action <= 2;
}

} // namespace manager::application::shortcuts
