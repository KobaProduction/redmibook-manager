#pragma once
#include "application/battery/charge_control.hpp"
#include "application/microphone/firmware_state_port.hpp"
#include "application/performance/performance_control.hpp"
#include <memory>
namespace manager::adapters {
std::shared_ptr<application::battery::ChargeControl> make_core_charge_control();
std::shared_ptr<application::performance::PerformanceControl> make_core_performance_control();
std::shared_ptr<application::microphone::FirmwareStatePort> make_core_microphone_state();
} // namespace manager::adapters
