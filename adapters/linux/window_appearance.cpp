#include "adapters/platform/window_appearance.hpp"
namespace manager::adapters {
bool request_window_foreground(std::uintptr_t) {
    return false;
}
bool set_window_rounding(std::uintptr_t, bool) {
    return false;
}
} // namespace manager::adapters
