#pragma once
#include <array>
#include <string_view>

namespace manager::application::presets {
inline constexpr std::array<std::string_view, 25> icon_ids{
    "balanced",
    "quiet",
    "turbo",
    "display",
    "battery-limit",
    "manager",
    "microphone-active",
    "microphone-muted",
    "battery",
    "plug",
    "power",
    "moon",
    "sun",
    "fan",
    "volume",
    "headphones",
    "keyboard",
    "wifi",
    "bluetooth",
    "lock",
    "gamepad",
    "video",
    "code",
    "briefcase",
    "coffee",
};
} // namespace manager::application::presets
