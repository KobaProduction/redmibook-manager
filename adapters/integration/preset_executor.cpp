#include "adapters/integration/preset_executor.hpp"
#include "application/display/cycle_refresh.hpp"
#include <algorithm>

namespace manager::adapters {
using application::presets::Kind;
namespace {
void append_refresh(application::presets::Catalog &result, const application::display::Snapshot &display) {
    if (display.status != application::display::Status::Ready)
        return;
    const auto *panel = application::display::unique_internal_panel(display);
    if (!panel)
        return;
    for (const auto hz : panel->available_hz)
        result.push_back({{Kind::Refresh, std::to_string(hz)}, {}});
}
void append_schemes(application::presets::Catalog &result, const application::power::Snapshot &schemes) {
    if (schemes.status != application::power::Status::Ready)
        return;
    for (const auto &scheme : schemes.schemes)
        result.push_back({{Kind::PowerScheme, scheme.id}, scheme.name});
}
void append_brightness(application::presets::Catalog &result,
                       application::display::BrightnessPort *brightness) {
    if (!brightness || brightness->read().status != application::display::BrightnessStatus::Ready)
        return;
    for (unsigned int value = 0; value <= 100; ++value)
        result.push_back({{Kind::Brightness, std::to_string(value)}, {}});
}
} // namespace
PresetExecutor::PresetExecutor(std::shared_ptr<application::performance::PerformanceControl> performance,
                               std::shared_ptr<application::display::RefreshPort> refresh,
                               std::shared_ptr<application::power::SchemePort> power,
                               std::shared_ptr<application::display::BrightnessPort> brightness,
                               std::shared_ptr<application::power::SaverPort> saver)
    : performance_(std::move(performance)), refresh_(std::move(refresh)), power_(std::move(power)),
      brightness_(std::move(brightness)), saver_(std::move(saver)) {}
application::presets::Catalog PresetExecutor::choices() {
    application::presets::Catalog result;
    if (performance_->capability() == application::performance::Status::Available)
        for (int profile = 1; profile <= 3; ++profile)
            result.push_back({{Kind::Performance, std::to_string(profile)}, {}});
    append_refresh(result, refresh_->read());
    append_schemes(result, power_->read());
    append_brightness(result, brightness_.get());
    if (saver_ && saver_->read().controllable) {
        result.push_back({{Kind::EnergySaver, "0"}, {}});
        result.push_back({{Kind::EnergySaver, "1"}, {}});
    }
    return result;
}
bool PresetExecutor::apply(const application::presets::Action &action) {
    if (action.kind == Kind::Brightness)
        return brightness_ && brightness_->set(static_cast<unsigned int>(std::stoul(action.value))).status ==
                                  application::display::BrightnessStatus::Ready;
    if (action.kind == Kind::EnergySaver)
        return saver_ && saver_->set(action.value == "1").status == application::power::SaverStatus::Ready;
    if (action.kind == Kind::PowerScheme)
        return power_->select(action.value) == application::power::Status::Ready;
    if (action.kind == Kind::Performance) {
        const auto profile = static_cast<application::performance::Profile>(std::stoi(action.value));
        return application::performance::apply_profile(*performance_, profile).status ==
               application::performance::Status::Confirmed;
    }
    return apply_refresh(action.value);
}
bool PresetExecutor::apply_refresh(const std::string &value) {
    const auto snapshot = refresh_->read();
    const auto *panel = application::display::unique_internal_panel(snapshot);
    const auto hz = static_cast<unsigned int>(std::stoul(value));
    if (snapshot.status != application::display::Status::Ready || !panel ||
        std::ranges::find(panel->available_hz, hz) == panel->available_hz.end())
        return false;
    const auto after = refresh_->set_refresh(panel->id, hz);
    return after.status == application::display::Status::Ready && after.observed_hz == hz;
}
} // namespace manager::adapters
