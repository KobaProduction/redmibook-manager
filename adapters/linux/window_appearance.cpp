#include "adapters/platform/window_appearance.hpp"
namespace manager::adapters {
void raise_osd_window(std::uintptr_t) {
    // Qt owns the topmost hint; the compositor decides whether it can be honored.
}
bool request_window_foreground(std::uintptr_t) {
    return false;
}
bool set_window_rounding(std::uintptr_t, bool) {
    return false;
}
} // namespace manager::adapters
