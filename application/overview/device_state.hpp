#pragma once
#include "application/battery/charge_control.hpp"
#include "application/display/refresh_port.hpp"
#include "application/microphone/control_port.hpp"
#include "application/performance/performance_control.hpp"
#include <cstdint>

namespace manager::application::overview {
// Read projection only. It owns no hardware policies or executable commands.
struct DeviceState {
    std::uint64_t revision{};
    microphone::Observation microphone;
    display::Snapshot displays;
    battery::Observation battery;
    battery::Capability battery_capability;
    performance::Observation performance;
    performance::Status performance_capability{performance::Status::Unavailable};
};
} // namespace manager::application::overview
