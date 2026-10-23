#pragma once
#include "application/display/brightness_port.hpp"
#include "application/power/saver_port.hpp"
#include <memory>

namespace manager::adapters {
std::shared_ptr<application::display::BrightnessPort> make_brightness();
std::shared_ptr<application::power::SaverPort> make_energy_saver();
} // namespace manager::adapters
