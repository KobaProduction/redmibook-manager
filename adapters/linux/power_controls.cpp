#include "adapters/platform/power_controls.hpp"

namespace manager::adapters {

std::shared_ptr<application::display::BrightnessPort> make_brightness() {
    return {};
}
std::shared_ptr<application::power::SaverPort> make_energy_saver() {
    return {};
}
} // namespace manager::adapters
