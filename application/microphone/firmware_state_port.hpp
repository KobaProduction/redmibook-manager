#pragma once
#include <cstdint>
#include <optional>
#include <string>
namespace manager::application::microphone {
enum class FirmwareStateStatus : std::uint8_t {
    Ready,
    NotImplemented,
    UnsupportedHardware,
    UnverifiedFirmware,
    Denied,
    Unavailable,
    Incompatible,
    Failed,
    OutcomeUnknown
};
struct FirmwareState {
    FirmwareStateStatus status{FirmwareStateStatus::Unavailable};
    std::optional<bool> desired_muted;
    std::string detail;
    bool operator==(const FirmwareState &) const = default;
};
// Firmware intent is not Windows endpoint mute or physical LED readback.
class FirmwareStatePort {
  public:
    virtual ~FirmwareStatePort() = default;
    virtual FirmwareState read() const = 0;
    virtual FirmwareState request(bool) const {
        return {FirmwareStateStatus::NotImplemented, {}, "firmware-microphone-control-absent"};
    }
};
// Only known qualified firmware intent may reconcile startup; newer events supersede this GET.
inline bool apply_initial_state(const FirmwareState &state, bool newer_event) noexcept {
    return !newer_event && state.status == FirmwareStateStatus::Ready && state.desired_muted.has_value();
}
} // namespace manager::application::microphone
