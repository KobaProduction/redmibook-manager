#pragma once
#include "application/shortcuts/input_port.hpp"
#include <memory>

namespace manager::adapters {
// Requires a live Qt GUI event loop on the calling thread.
std::unique_ptr<application::shortcuts::InputPort> make_native_input();
} // namespace manager::adapters
