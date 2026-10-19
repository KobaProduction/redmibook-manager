#pragma once
#include "application/battery/charge_control.hpp"
#include "application/performance/performance_control.hpp"
#include <memory>
namespace manager::adapters {
std::shared_ptr<application::battery::ChargeControl> make_core_charge_control();
std::shared_ptr<application::performance::PerformanceControl> make_core_performance_control();
} // namespace manager::adapters
