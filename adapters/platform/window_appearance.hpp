#pragma once
#include <cstdint>
namespace manager::adapters {
bool set_window_rounding(std::uintptr_t handle, bool rounded);
bool request_window_foreground(std::uintptr_t handle);
} // namespace manager::adapters
