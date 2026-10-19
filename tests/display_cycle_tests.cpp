#include "application/display/cycle_refresh.hpp"
#include <iostream>

namespace {
namespace display = manager::application::display;
struct Refresh final : display::RefreshPort {
    display::Snapshot snapshot{display::Status::Ready, {}, {}};
    mutable std::string changed;
    mutable unsigned int requested{};
    display::Snapshot read() const override {
        return snapshot;
    }
    display::Change set_refresh(const std::string &id, unsigned int hz) const override {
        changed = id;
        requested = hz;
        return {display::Status::Ready, hz, {}};
    }
};
bool internal_only() {
    Refresh control;
    control.snapshot.displays = {{"external", "", 1920, 1080, 144, {60, 144}, false},
                                 {"laptop", "", 3200, 2000, 60, {120, 48, 60}, true}};
    if (display::cycle_internal_refresh(control).observed_hz != 120 || control.changed != "laptop")
        return false;
    control.snapshot.displays[1].current_hz = 120;
    return display::cycle_internal_refresh(control).observed_hz == 60;
}
bool refuses_ambiguous_or_missing() {
    Refresh control;
    control.snapshot.displays = {{"external", "", 1920, 1080, 60, {60, 120}, false}};
    if (display::cycle_internal_refresh(control).status != display::Status::Unavailable ||
        !control.changed.empty())
        return false;
    control.snapshot.displays[0].internal_panel = true;
    control.snapshot.displays.push_back(control.snapshot.displays.front());
    return display::cycle_internal_refresh(control).status == display::Status::Unavailable &&
           control.changed.empty();
}
} // namespace
int main() {
    if (!internal_only() || !refuses_ambiguous_or_missing()) {
        std::cerr << "Internal panel cycle contract failed\n";
        return 1;
    }
}
