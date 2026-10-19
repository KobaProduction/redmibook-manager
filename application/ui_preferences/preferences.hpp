#pragma once

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
};

inline bool valid_preferences(const UiPreferences &value) {
    const bool language = value.language == "system" || value.language == "en" || value.language == "ru";
    const bool theme = value.theme == "system" || value.theme == "light" || value.theme == "dark";
    const bool position =
        std::find(osd_positions.begin(), osd_positions.end(), value.osd_position) != osd_positions.end();
    const bool design = value.design == "fluent" || value.design == "classic";
    return language && theme && position && design && !value.osd_screen.empty() &&
           value.osd_screen.size() <= 256;
}
} // namespace manager::application
