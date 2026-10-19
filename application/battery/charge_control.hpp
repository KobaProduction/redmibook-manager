#pragma once

#include "domain/battery/charge_limit.hpp"

#include <optional>
#include <string>
#include <vector>

namespace manager::application::battery {

using domain::battery::ChargeLimit;

enum class Status : std::uint8_t {
    Available,
    NotImplemented,
    UnsupportedHardware,
    UnverifiedFirmware,
    PermissionDenied,
    Unavailable,
    Failed,
    InvalidRequest,
    AppliedUnconfirmed,
    Confirmed,
    OutcomeUnknown
};

struct Capability {
    Status status{Status::Unavailable};
    std::vector<ChargeLimit> allowed_limits;
    std::string detail;
};

struct Observation {
    Status status{Status::Unavailable};
    std::optional<ChargeLimit> limit;
    std::string detail;
};

// Synchronous, caller-owned and thread-confined. Read never changes hardware.
// Confirmation means firmware policy readback, not observed physical charge cutoff.
class ChargeControl {
  public:
    virtual ~ChargeControl() = default;
    [[nodiscard]] virtual Capability capability() const = 0;
    [[nodiscard]] virtual Observation read() = 0;
    [[nodiscard]] virtual Observation request(ChargeLimit limit) = 0;
};

[[nodiscard]] Observation apply_charge_limit(ChargeControl &control, ChargeLimit limit);

} // namespace manager::application::battery
