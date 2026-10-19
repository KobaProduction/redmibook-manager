#pragma once

#include "application/display/refresh_selection.hpp"
#include "application/shortcuts/binding_action.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <string>
#include <string_view>

namespace manager::application {
inline constexpr std::array<std::string_view, 9> osd_positions{
    "top-left",     "top-center",  "top-right",     "center-left", "center",
    "center-right", "bottom-left", "bottom-center", "bottom-right"};
struct UiPreferences {
    std::string language{"system"};
    std::string theme{"system"};
    std::uint64_t revision{0};
    std::string osd_position{"top-left"};
    bool tray_autostart{true};
    std::string osd_screen{"auto"};
    std::string design{"fluent"};
    bool auto_updates{true};
    bool rounded_corners{true};
    std::vector<unsigned int> refresh_cycle_rates{60, 165};
    shortcuts::BindingAction fn_s_action{shortcuts::BindingAction::Default};
    shortcuts::BindingAction fn_k_action{shortcuts::BindingAction::Default};
};

inline bool valid_preferences(const UiPreferences &value) {
    const bool language = value.language == "system" || value.language == "en" || value.language == "ru";
    const bool theme = value.theme == "system" || value.theme == "light" || value.theme == "dark";
    const bool position =
        std::find(osd_positions.begin(), osd_positions.end(), value.osd_position) != osd_positions.end();
    const bool design = value.design == "fluent" || value.design == "classic";
    return language && theme && position && design && !value.osd_screen.empty() &&
           value.osd_screen.size() <= 256 && display::valid_refresh_selection(value.refresh_cycle_rates) &&
           shortcuts::valid_binding_action(static_cast<int>(value.fn_s_action)) &&
           shortcuts::valid_binding_action(static_cast<int>(value.fn_k_action));
}
} // namespace manager::application
