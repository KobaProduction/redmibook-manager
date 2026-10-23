#pragma once

#include "application/display/refresh_selection.hpp"
#include "application/presets/model.hpp"
#include "application/shortcuts/binding_action.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <string>
#include <string_view>

namespace manager::application {
inline constexpr std::array<std::string_view, 6> tray_section_ids{"performance",  "refresh",    "battery",
                                                                  "batteryPower", "microphone", "connection"};
inline bool valid_battery_interval(int value) {
    return value == 100 || value == 1000 || value == 5000;
}
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
    bool osd_sound{true};
    bool osd_enabled{true};
    // Empty means device-advertised standard choices; it never selects a physical mode.
    std::vector<unsigned int> refresh_cycle_rates{};
    shortcuts::BindingAction fn_s_action{shortcuts::BindingAction::Default};
    shortcuts::BindingAction vendor_key_action{shortcuts::BindingAction::Default};
    int battery_interval_ms{1000};
    std::vector<std::string> tray_sections{"performance",  "refresh",    "battery",
                                           "batteryPower", "microphone", "connection"};
    std::vector<presets::Preset> tray_presets{};
};
inline bool valid_tray_sections(const std::vector<std::string> &sections) {
    if (sections.size() > tray_section_ids.size())
        return false;
    for (std::size_t index = 0; index < sections.size(); ++index) {
        if (std::find(tray_section_ids.begin(), tray_section_ids.end(), sections[index]) ==
            tray_section_ids.end())
            return false;
        if (std::find(sections.begin(), sections.begin() + static_cast<std::ptrdiff_t>(index),
                      sections[index]) != sections.begin() + static_cast<std::ptrdiff_t>(index))
            return false;
    }
    return true;
}

inline bool valid_preferences(const UiPreferences &value) {
    const bool language = value.language == "system" || value.language == "en" || value.language == "ru";
    const bool theme = value.theme == "system" || value.theme == "light" || value.theme == "dark";
    const bool position =
        std::find(osd_positions.begin(), osd_positions.end(), value.osd_position) != osd_positions.end();
    const bool design = value.design == "fluent" || value.design == "classic";
    return language && theme && position && design && valid_battery_interval(value.battery_interval_ms) &&
           valid_tray_sections(value.tray_sections) && presets::valid(value.tray_presets) &&
           !value.osd_screen.empty() && value.osd_screen.size() <= 256 &&
           (value.refresh_cycle_rates.empty() ||
            display::valid_refresh_selection(value.refresh_cycle_rates)) &&
           shortcuts::valid_binding_action(static_cast<int>(value.fn_s_action)) &&
           shortcuts::valid_binding_action(static_cast<int>(value.vendor_key_action));
}
} // namespace manager::application
