#pragma once

#include "domain/performance/profile.hpp"

#include <optional>
#include <string>

namespace manager::application::performance {

using domain::performance::Profile;
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

struct Observation {
    Status status{Status::Unavailable};
    std::optional<Profile> profile;
    std::string detail;
    bool operator==(const Observation &) const = default;
};

// Synchronous and caller-owned; execute off the GUI thread. Read is side-effect-free.
// Confirmed means the firmware-reported profile matches, not measured CPU power/fan behavior.
class PerformanceControl {
  public:
    virtual ~PerformanceControl() = default;
    [[nodiscard]] virtual Status capability() const = 0;
    [[nodiscard]] virtual Observation read() = 0;
    [[nodiscard]] virtual Observation request(Profile profile) = 0;
};

[[nodiscard]] Observation apply_profile(PerformanceControl &control, Profile profile);

} // namespace manager::application::performance
