#include "adapters/platform/firmware_controls.hpp"
#include "adapters/linux/battery/charge_control.hpp"
#include "adapters/linux/performance/performance_control.hpp"
namespace manager::adapters {
std::shared_ptr<application::microphone::FirmwareStatePort> make_core_microphone_state() {
    return {};
}
std::shared_ptr<application::battery::ChargeControl> make_core_charge_control() {
    return std::make_shared<linux_battery::ChargeControl>();
}
std::shared_ptr<application::performance::PerformanceControl> make_core_performance_control() {
    return std::make_shared<linux_performance::PerformanceControl>();
}
} // namespace manager::adapters
