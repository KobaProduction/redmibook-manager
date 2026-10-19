#pragma once

#include <cstdint>

namespace manager::domain::performance {

enum class Profile : std::uint8_t { Balanced = 1, Quiet = 2, Turbo = 3, FullSpeed = 4 };

} // namespace manager::domain::performance
