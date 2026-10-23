#pragma once
#include "application/battery/charge_control.hpp"
#include "application/battery/telemetry.hpp"
#include "application/display/refresh_port.hpp"
#include "application/microphone/control_port.hpp"
#include "application/microphone/firmware_state_port.hpp"
#include "application/performance/performance_control.hpp"
#include <cstdint>

namespace manager::application::overview {
enum class RefreshResult : std::uint8_t { Completed, Failed, TimedOut, Unavailable };
// Read projection only. It owns no hardware policies or executable commands.
struct DeviceState {
    std::uint64_t revision{};
    microphone::Observation microphone;
    display::Snapshot displays;
    battery::Observation battery;
    battery::Telemetry battery_telemetry;
    battery::Capability battery_capability;
    performance::Observation performance;
    performance::Status performance_capability{performance::Status::Unavailable};
    microphone::FirmwareState microphone_firmware;
    bool operator==(const DeviceState &) const = default;
};
} // namespace manager::application::overview
