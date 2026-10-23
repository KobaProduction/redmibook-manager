#pragma once
#include "application/microphone/control_port.hpp"
#include "application/microphone/firmware_state_port.hpp"
#include <functional>
namespace manager::application::microphone {
// Firmware events are invalidations, never requests to write firmware again.
inline Observation synchronize(const FirmwareStatePort &firmware, const ControlPort &endpoint,
                               const std::function<bool()> &current_intent) {
    const auto desired = firmware.read();
    if (desired.status != FirmwareStateStatus::Ready || !desired.desired_muted)
        return {Status::Failed, {}, "firmware-microphone-state-unavailable"};
    auto current = endpoint.read();
    if (current.status != Status::Ready || !current.muted)
        return current;
    if (!current_intent())
        return {Status::Failed, {}, "firmware-microphone-intent-superseded"};
    if (current.muted == desired.desired_muted)
        return current;
    return set_muted_confirmed(endpoint, *desired.desired_muted);
}
} // namespace manager::application::microphone
