#include "application/battery/charge_policy.hpp"
#include <iostream>
#include <limits>
#include <vector>

namespace {
namespace battery = manager::application::battery;
using battery::ChargeLimit;
using battery::Status;
class Store final : public battery::PolicyStore {
  public:
    battery::Policy value{ChargeLimit::Protection80, true, false};
    bool writable{true};
    battery::Policy load() override {
        return value;
    }
    bool save(ChargeLimit limit) override {
        if (writable)
            value.desired = limit;
        return writable;
    }
    bool mark_unsettled(bool pending) override {
        if (writable)
            value.unsettled = pending;
        return writable;
    }
};
class Control final : public battery::ChargeControl {
  public:
    explicit Control(Store &store) : store_(store) {}
    std::vector<ChargeLimit> writes;
    ChargeLimit current{ChargeLimit::Protection80};
    int fail_at{};
    bool journal_preserved{true};
    bool matching_readback{true};
    Status read_status{Status::Available};
    Status capability_status{Status::Available};
    battery::Capability capability() const override {
        return {capability_status, {ChargeLimit::Full, ChargeLimit::Protection80}, {}};
    }
    battery::Observation read() override {
        return {read_status, current, {}};
    }
    battery::Observation request(ChargeLimit limit) override {
        writes.push_back(limit);
        journal_preserved =
            journal_preserved && store_.value.unsettled && store_.value.desired == ChargeLimit::Protection80;
        if (static_cast<int>(writes.size()) == fail_at)
            return {Status::OutcomeUnknown, {}, {}};
        if (matching_readback)
            current = limit;
        return {Status::AppliedUnconfirmed, {}, {}};
    }

  private:
    Store &store_;
};
battery::Telemetry charging(int percent = 80) {
    battery::Telemetry result;
    result.status = battery::TelemetryStatus::Available;
    result.online = true;
    result.charging = true;
    result.percent = percent;
    result.rate_w = 20;
    return result;
}
bool recovery(int percent) {
    Store store;
    Control control(store);
    battery::MaintainChargePolicy policy(store);
    const auto result = policy.guard(control, charging(percent));
    return result.status == Status::Confirmed && result.limit == ChargeLimit::Protection80 &&
           control.writes == std::vector{ChargeLimit::Full, ChargeLimit::Protection80} &&
           control.journal_preserved && !store.value.unsettled &&
           store.value.desired == ChargeLimit::Protection80;
}
template <class Field, class Value>
void append_sample(std::vector<battery::Telemetry> &samples, Field battery::Telemetry::*field, Value value) {
    auto sample = charging();
    sample.*field = value;
    samples.push_back(sample);
}
bool safe_samples() {
    std::vector<battery::Telemetry> samples{charging(79), charging(0), charging(101)};
    append_sample(samples, &battery::Telemetry::percent, std::nullopt);
    append_sample(samples, &battery::Telemetry::charging, false);
    append_sample(samples, &battery::Telemetry::online, false);
    append_sample(samples, &battery::Telemetry::rate_w, 0.0);
    append_sample(samples, &battery::Telemetry::rate_w, -20.0);
    append_sample(samples, &battery::Telemetry::rate_w, std::nullopt);
    append_sample(samples, &battery::Telemetry::rate_w, std::numeric_limits<double>::quiet_NaN());
    append_sample(samples, &battery::Telemetry::status, battery::TelemetryStatus::Failed);
    Store store;
    Control control(store);
    battery::MaintainChargePolicy policy(store);
    for (const auto &value : samples)
        (void)policy.guard(control, value);
    return control.writes.empty();
}
bool unknown_stops(int phase) {
    Store store;
    Control control(store);
    control.fail_at = phase;
    battery::MaintainChargePolicy policy(store);
    if (policy.guard(control, charging()).status != Status::OutcomeUnknown)
        return false;
    battery::MaintainChargePolicy restarted(store);
    (void)restarted.guard(control, charging());
    (void)restarted.reconcile(control);
    return static_cast<int>(control.writes.size()) == phase && control.journal_preserved &&
           store.value.unsettled && store.value.desired == ChargeLimit::Protection80;
}
bool missing_confirmation() {
    Store store;
    Control control(store);
    control.matching_readback = false;
    battery::MaintainChargePolicy policy(store);
    return policy.guard(control, charging()).status == Status::OutcomeUnknown && control.writes.size() == 1 &&
           store.value.unsettled;
}
bool policy_gates() {
    Store store;
    Control control(store);
    battery::MaintainChargePolicy policy(store);
    store.value.desired.reset();
    (void)policy.guard(control, charging());
    store.value.desired = ChargeLimit::Full;
    (void)policy.guard(control, charging());
    store.value.desired = ChargeLimit::Protection80;
    store.value.valid = false;
    (void)policy.guard(control, charging());
    store.value.valid = true;
    control.read_status = Status::Failed;
    (void)policy.guard(control, charging());
    control.read_status = Status::Available;
    control.capability_status = Status::UnverifiedFirmware;
    (void)policy.guard(control, charging());
    control.capability_status = Status::Available;
    store.writable = false;
    (void)policy.guard(control, charging());
    return control.writes.empty();
}
bool drift() {
    Store store;
    Control control(store);
    control.current = ChargeLimit::Full;
    battery::MaintainChargePolicy policy(store);
    return policy.guard(control, charging()).status == Status::Confirmed &&
           control.writes == std::vector{ChargeLimit::Protection80} && control.journal_preserved;
}
} // namespace
int main() {
    if (!recovery(80) || !recovery(81) || !safe_samples() || !unknown_stops(1) || !unknown_stops(2) ||
        !missing_confirmation() || !policy_gates() || !drift()) {
        std::cerr << "Charge recovery contract failed\n";
        return 1;
    }
}
