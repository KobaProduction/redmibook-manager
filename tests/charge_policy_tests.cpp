#include "application/battery/charge_policy.hpp"
#include <iostream>
namespace {
namespace battery = manager::application::battery;
class Store final : public battery::PolicyStore {
  public:
    battery::Policy value;
    bool writable{true};
    battery::Policy load() override {
        return value;
    }
    bool save(battery::ChargeLimit limit) override {
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
    int writes{};
    battery::Status read_status{battery::Status::Available};
    battery::Status write_status{battery::Status::AppliedUnconfirmed};
    battery::ChargeLimit current{battery::ChargeLimit::Full};
    battery::Capability capability() const override {
        return {
            battery::Status::Available, {battery::ChargeLimit::Full, battery::ChargeLimit::Protection80}, {}};
    }
    battery::Observation read() override {
        return {read_status, current, {}};
    }
    battery::Observation request(battery::ChargeLimit limit) override {
        ++writes;
        if (write_status == battery::Status::AppliedUnconfirmed)
            current = limit;
        return {write_status, {}, {}};
    }
};
class Audit final : public battery::PolicyAudit {
  public:
    int drifts{};
    void record(std::string_view, const battery::Observation &before,
                std::optional<battery::ChargeLimit> desired,
                const battery::Observation &after) noexcept override {
        if (before.limit && desired && before.limit != desired &&
            after.status == battery::Status::Confirmed && after.limit == desired)
            ++drifts;
    }
};
bool recover_charger_reset(Store &store, Control &control, Audit &audit) {
    control.current = battery::ChargeLimit::Full; // Charger/EC reset.
    battery::MaintainChargePolicy restarted(store, &audit);
    if (restarted.reconcile(control).status != battery::Status::Confirmed || control.writes != 2)
        return false;
    control.current = battery::ChargeLimit::Full;
    control.read_status = battery::Status::Failed;
    (void)restarted.reconcile(control);
    return control.writes == 2 && audit.drifts == 1;
}
bool retention() {
    Store store;
    Control control;
    Audit audit;
    battery::MaintainChargePolicy policy(store, &audit);
    (void)policy.reconcile(control);
    if (control.writes != 0 || store.value.desired)
        return false;
    if (policy.select(control, battery::ChargeLimit::Protection80).status != battery::Status::Confirmed)
        return false;
    (void)policy.reconcile(control);
    if (control.writes != 1 || store.value.unsettled)
        return false;
    return recover_charger_reset(store, control, audit);
}
bool unknown_is_durable() {
    Store store;
    Control control;
    control.write_status = battery::Status::OutcomeUnknown;
    battery::MaintainChargePolicy policy(store);
    if (policy.select(control, battery::ChargeLimit::Protection80).status != battery::Status::OutcomeUnknown)
        return false;
    battery::MaintainChargePolicy restarted(store);
    (void)restarted.reconcile(control);
    (void)restarted.select(control, battery::ChargeLimit::Full);
    return control.writes == 1 && store.value.unsettled;
}
bool unavailable_storage() {
    Store store;
    store.writable = false;
    Control control;
    battery::MaintainChargePolicy policy(store);
    return policy.select(control, battery::ChargeLimit::Protection80).status == battery::Status::Failed &&
           control.writes == 0;
}
} // namespace
int main() {
    if (!retention() || !unknown_is_durable() || !unavailable_storage()) {
        std::cerr << "Charge policy retention contract failed\n";
        return 1;
    }
}
