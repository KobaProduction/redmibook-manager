#pragma once
#include "application/presets/starter.hpp"
namespace manager::preview {
inline application::presets::Catalog demo_catalog() {
    using application::presets::Kind;
    return {{{Kind::Performance, "1"}, {}},
            {{Kind::Performance, "2"}, {}},
            {{Kind::Performance, "3"}, {}},
            {{Kind::Refresh, "60"}, {}},
            {{Kind::Refresh, "120"}, {}},
            {{Kind::Refresh, "165"}, {}},
            {{Kind::EnergySaver, "0"}, {}},
            {{Kind::EnergySaver, "1"}, {}},
            {{Kind::Brightness, "50"}, {}},
            {{Kind::Brightness, "100"}, {}},
            {{Kind::PowerScheme, "{381b4222-f694-41f0-9685-ff5bb260df2e}"}, "Balanced"},
            {{Kind::PowerScheme, "{8c5e7fda-e8bf-4a96-9a85-a6e23a8c635c}"}, "High performance"}};
}
inline std::vector<application::presets::Preset> demo_presets() {
    return {application::presets::starter(demo_catalog())};
}
} // namespace manager::preview
