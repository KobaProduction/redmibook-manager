#include "apps/tray/device_controls.hpp"
#include <chrono>
namespace manager::tray {
namespace {
std::optional<application::presets::Button>
find_button(const std::vector<application::presets::Preset> &presets, const QString &preset,
            const QString &button) {
    for (const auto &item : presets) {
        if (item.id != preset.toStdString() || !item.visible)
            continue;
        for (const auto &entry : item.buttons)
            if (entry.id == button.toStdString() && !entry.actions.empty())
                return entry;
    }
    return std::nullopt;
}
} // namespace
void DeviceControls::apply_preset(const QString &preset, const QString &button) {
    if (busy_ || !connected_)
        return;
    const auto entry = find_button(preferences_.tray_presets(), preset, button);
    if (!entry)
        return;
    preset_result_ = std::make_shared<application::presets::Result>();
    run(
        [executor = preset_executor_, config = *entry, result = preset_result_,
         until = std::chrono::steady_clock::now() + std::chrono::seconds(10)] {
            *result = application::presets::execute(
                config, *executor, [until] { return std::chrono::steady_clock::now() < until; });
            return result->confirmed;
        },
        {application::feedback::Kind::Performance, 1}, true);
}
} // namespace manager::tray
