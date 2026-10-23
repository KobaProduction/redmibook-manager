#pragma once
#include "application/battery/telemetry.hpp"

namespace manager::adapters {
std::shared_ptr<application::battery::TelemetryPort> make_battery_telemetry();
// Destruction unregisters callbacks before releasing their captured state.
std::unique_ptr<application::battery::PowerSubscription> watch_power(const std::function<void()> &changed,
                                                                     bool observe_level = true);
} // namespace manager::adapters
