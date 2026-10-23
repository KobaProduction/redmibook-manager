#pragma once
#include "application/presets/model.hpp"
#include <algorithm>
#include <utility>

namespace manager::application::presets {
inline constexpr int starter_version = 2;
namespace detail {
inline void append_supported(Button &button, Action action, const Catalog &catalog) {
    if (std::ranges::any_of(catalog, [&](const auto &choice) { return choice.action == action; }))
        button.actions.push_back(std::move(action));
}
inline void append_max_refresh(Button &button, const Catalog &catalog,
                               const std::vector<unsigned int> &selected) {
    for (const unsigned int rate : {165U, 120U, 60U}) {
        if (!selected.empty() && std::ranges::find(selected, rate) == selected.end())
            continue;
        append_supported(button, {Kind::Refresh, std::to_string(rate)}, catalog);
        if (!button.actions.empty())
            break;
    }
}
inline void migrate_button(Button &button) {
    if (button.default_title != DefaultTitle::None)
        return;
    if (button.id == "starter-min" && button.title == "Min") {
        button.title = "Economy";
        button.default_title = DefaultTitle::Economy;
    } else if (button.id == "starter-max" && button.title == "Max") {
        button.title = "Performance";
        button.default_title = DefaultTitle::Performance;
    }
}
} // namespace detail
// Creation only: never applies actions or changes the user's refresh-rate choices.
inline Preset starter(const Catalog &catalog, const std::vector<unsigned int> &selected = {}) {
    Preset result{"starter-energy",
                  "Power",
                  false,
                  {{"starter-min", "Economy", "quiet", {}, DefaultTitle::Economy},
                   {"starter-max", "Performance", "turbo", {}, DefaultTitle::Performance}},
                  DefaultTitle::Power};
    auto &minimum = result.buttons.front();
    auto &maximum = result.buttons.back();
    detail::append_supported(minimum, {Kind::Refresh, "60"}, catalog);
    detail::append_max_refresh(maximum, catalog, selected);
    detail::append_supported(minimum, {Kind::EnergySaver, "1"}, catalog);
    detail::append_supported(minimum, {Kind::Performance, "2"}, catalog);
    detail::append_supported(minimum, {Kind::Brightness, "50"}, catalog);
    detail::append_supported(maximum, {Kind::EnergySaver, "0"}, catalog);
    detail::append_supported(maximum, {Kind::Performance, "3"}, catalog);
    detail::append_supported(maximum, {Kind::Brightness, "100"}, catalog);
    return result;
}
inline void migrate_starter(Preset &preset) {
    if (preset.id != "starter-energy")
        return;
    preset.visible = false;
    if (preset.title == "Energy" && preset.default_title == DefaultTitle::None) {
        preset.title = "Power";
        preset.default_title = DefaultTitle::Power;
    }
    std::ranges::for_each(preset.buttons, detail::migrate_button);
}
} // namespace manager::application::presets
