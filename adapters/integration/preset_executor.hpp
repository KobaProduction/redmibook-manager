#pragma once
#include "application/display/brightness_port.hpp"
#include "application/display/refresh_port.hpp"
#include "application/performance/performance_control.hpp"
#include "application/power/saver_port.hpp"
#include "application/power/scheme_port.hpp"
#include "application/presets/model.hpp"
#include <memory>

namespace manager::adapters {
class PresetExecutor final : public application::presets::ExecutorPort {
  public:
    PresetExecutor(std::shared_ptr<application::performance::PerformanceControl> performance,
                   std::shared_ptr<application::display::RefreshPort> refresh,
                   std::shared_ptr<application::power::SchemePort> power,
                   std::shared_ptr<application::display::BrightnessPort> brightness = nullptr,
                   std::shared_ptr<application::power::SaverPort> saver = nullptr);
    application::presets::Catalog choices() override;
    bool apply(const application::presets::Action &action) override;

  private:
    bool apply_refresh(const std::string &value);
    std::shared_ptr<application::performance::PerformanceControl> performance_;
    std::shared_ptr<application::display::RefreshPort> refresh_;
    std::shared_ptr<application::power::SchemePort> power_;
    std::shared_ptr<application::display::BrightnessPort> brightness_;
    std::shared_ptr<application::power::SaverPort> saver_;
};
} // namespace manager::adapters
