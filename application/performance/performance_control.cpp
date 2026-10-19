#include "application/performance/performance_control.hpp"

namespace manager::application::performance {

Observation apply_profile(PerformanceControl &control, Profile profile) {
    const auto supported = control.capability();
    if (supported != Status::Available)
        return {supported, {}, "Performance writes are unavailable for this firmware"};
    auto requested = control.request(profile);
    if (requested.status != Status::AppliedUnconfirmed)
        return requested;
    const auto observed = control.read();
    if (observed.status == Status::Available && observed.profile == profile)
        return {Status::Confirmed, profile, "Firmware policy readback matches"};
    return {Status::AppliedUnconfirmed, observed.profile,
            "Request accepted; matching policy readback unavailable"};
}

} // namespace manager::application::performance
