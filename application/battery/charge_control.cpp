#include "application/battery/charge_control.hpp"

#include <algorithm>

namespace manager::application::battery {

Observation apply_charge_limit(ChargeControl &control, ChargeLimit limit) {
    const auto capability = control.capability();
    if (capability.status != Status::Available)
        return {capability.status, {}, capability.detail};
    if (!domain::battery::valid(limit) ||
        std::ranges::find(capability.allowed_limits, limit) == capability.allowed_limits.end())
        return {Status::InvalidRequest, {}, "Charge limit is not advertised by this device"};
    auto requested = control.request(limit);
    if (requested.status != Status::AppliedUnconfirmed)
        return requested;
    const auto observed = control.read();
    if (observed.status == Status::Available && observed.limit == limit)
        return {Status::Confirmed, limit, "Firmware policy readback matches; physical cutoff is unverified"};
    return {Status::AppliedUnconfirmed, observed.limit,
            "Request accepted; matching policy readback unavailable"};
}

} // namespace manager::application::battery
