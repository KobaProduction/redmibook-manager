#include "application/battery/charge_policy.hpp"
#include <algorithm>
#include <cmath>

namespace manager::application::battery {
bool over_limit_charging(const Telemetry &sample) noexcept {
    return sample.status == TelemetryStatus::Available && sample.online == true && sample.charging == true &&
           sample.percent && *sample.percent >= 80 && *sample.percent <= 100 && sample.rate_w &&
           std::isfinite(*sample.rate_w) && *sample.rate_w > 0;
}

namespace {
std::optional<Observation> unsupported(const Capability &capability,
                                       std::initializer_list<ChargeLimit> limits, const char *detail) {
    if (capability.status != Status::Available)
        return Observation{capability.status, {}, capability.detail};
    for (const auto limit : limits)
        if (std::ranges::find(capability.allowed_limits, limit) == capability.allowed_limits.end())
            return Observation{Status::InvalidRequest, {}, detail};
    return {};
}
} // namespace
Observation MaintainChargePolicy::rearm_full(ChargeControl &control) {
    const auto full = apply_charge_limit(control, ChargeLimit::Full);
    if (audit_)
        audit_->record("over-limit-rearm-full", {}, ChargeLimit::Protection80, full);
    return full;
}

Observation MaintainChargePolicy::rearm(ChargeControl &control) {
    if (const auto error = unsupported(control.capability(), {ChargeLimit::Full, ChargeLimit::Protection80},
                                       "charge-rearm-not-supported"))
        return *error;
    if (!store_.mark_unsettled(true))
        return {Status::Failed, {}, "charge-policy-journal-failed"};
    // One durable transaction spans both writes. Persisted intent remains 80 throughout.
    unsettled_ = true;
    const auto full = rearm_full(control);
    if (full.status != Status::Confirmed)
        return {Status::OutcomeUnknown, full.limit, "charge-rearm-full-unconfirmed"};
    const auto restored = apply_charge_limit(control, ChargeLimit::Protection80);
    if (restored.status != Status::Confirmed || !store_.mark_unsettled(false))
        return {Status::OutcomeUnknown, restored.limit, "charge-rearm-restoration-unconfirmed"};
    unsettled_ = false;
    return {Status::Confirmed, ChargeLimit::Protection80,
            "Charge protection rearmed; physical cutoff awaits fresh telemetry"};
}

Observation MaintainChargePolicy::guard(ChargeControl &control, const Telemetry &sample) {
    const auto policy = store_.load();
    if (unsettled_ || policy.unsettled)
        return {Status::OutcomeUnknown, {}, "charge-policy-write-unsettled"};
    if (!policy.valid)
        return {Status::Failed, {}, "charge-policy-store-invalid"};
    if (policy.desired != ChargeLimit::Protection80 || !over_limit_charging(sample))
        return {Status::Available, {}, "charge-guard-not-required"};
    auto observed = control.read();
    if (observed.status != Status::Available || !observed.limit)
        return observed;
    const auto result = observed.limit == ChargeLimit::Protection80
                            ? rearm(control)
                            : apply(control, ChargeLimit::Protection80);
    if (audit_)
        audit_->record("over-limit-charging", observed, policy.desired, result);
    return result;
}

Observation MaintainChargePolicy::apply(ChargeControl &control, ChargeLimit limit) {
    if (!store_.mark_unsettled(true))
        return {Status::Failed, {}, "charge-policy-journal-failed"};
    auto result = apply_charge_limit(control, limit);
    unsettled_ = result.status != Status::Confirmed || !store_.mark_unsettled(false);
    if (unsettled_)
        return {Status::OutcomeUnknown, result.limit, "charge-policy-write-unsettled"};
    return result;
}
Observation MaintainChargePolicy::select(ChargeControl &control, ChargeLimit limit) {
    const auto saved = store_.load();
    if (unsettled_ || saved.unsettled)
        return {Status::OutcomeUnknown, {}, "charge-policy-write-unsettled"};
    if (!saved.valid)
        return {Status::Failed, {}, "charge-policy-store-invalid"};
    if (const auto error = unsupported(control.capability(), {limit}, "charge-limit-not-supported"))
        return *error;
    // Persist explicit intent before touching firmware. Failed storage forbids a write.
    if (!store_.save(limit))
        return {Status::Failed, {}, "charge-policy-save-failed"};
    const auto result = apply(control, limit);
    if (audit_)
        audit_->record("selection", {}, limit, result);
    return result;
}
Observation MaintainChargePolicy::reconcile(ChargeControl &control, std::string_view reason) {
    if (unsettled_)
        return {Status::OutcomeUnknown, {}, "charge-policy-write-unsettled"};
    const auto policy = store_.load();
    if (policy.unsettled)
        return {Status::OutcomeUnknown, {}, "charge-policy-write-unsettled"};
    if (!policy.valid)
        return {Status::Failed, {}, "charge-policy-store-invalid"};
    auto observed = control.read();
    const bool drift = policy.desired && observed.status == Status::Available && observed.limit &&
                       observed.limit != policy.desired;
    const auto result = drift ? apply(control, *policy.desired) : observed;
    if (audit_)
        audit_->record(reason, observed, policy.desired, result);
    return result;
}
} // namespace manager::application::battery
