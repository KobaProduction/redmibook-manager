#pragma once

#include <cstdint>

namespace manager::domain::battery {

enum class ChargeLimit : std::uint8_t { Protection80 = 80, Full = 100 };

[[nodiscard]] constexpr bool valid(ChargeLimit limit) noexcept {
    return limit == ChargeLimit::Protection80 || limit == ChargeLimit::Full;
}

} // namespace manager::domain::battery
