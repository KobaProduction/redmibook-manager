#pragma once
#include "application/display/refresh_port.hpp"
#include "application/display/refresh_selection.hpp"
#include <algorithm>

namespace manager::application::display {
inline const Display *unique_internal_panel(const Snapshot &snapshot) {
    const Display *panel = nullptr;
    for (const auto &display : snapshot.displays) {
        if (!display.internal_panel)
            continue;
        if (panel)
            return nullptr;
        panel = &display;
    }
    return panel;
}
// A laptop key targets its built-in panel, never whichever monitor is primary.
inline Change cycle_internal_refresh(const RefreshPort &control,
                                     const std::vector<unsigned int> &configured = {60, 165}) {
    const auto snapshot = control.read();
    if (snapshot.status != Status::Ready)
        return {snapshot.status, 0, snapshot.detail};
    const auto *panel = unique_internal_panel(snapshot);
    if (!panel)
        return {Status::Unavailable, 0, "internal-panel-refresh-unavailable"};
    const auto selected = selected_refresh_rates(panel->available_hz, configured);
    const auto next = next_refresh_rate(panel->current_hz, selected);
    if (!next)
        return {Status::Unavailable, 0, "selected-refresh-rates-unavailable"};
    return control.set_refresh(panel->id, *next);
}
} // namespace manager::application::display
