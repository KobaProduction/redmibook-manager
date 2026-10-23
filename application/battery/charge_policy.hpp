#pragma once
#include "application/battery/charge_control.hpp"
#include "application/battery/telemetry.hpp"
#include <string_view>

namespace manager::application::battery {
[[nodiscard]] bool over_limit_charging(const Telemetry &sample) noexcept;
struct Policy {
    std::optional<ChargeLimit> desired;
    bool valid{true};
    bool unsettled{false};
};
class PolicyStore {
  public:
    virtual ~PolicyStore() = default;
    virtual Policy load() = 0;
    virtual bool save(ChargeLimit limit) = 0;
    virtual bool mark_unsettled(bool value) = 0;
};
class PolicyAudit {
  public:
    virtual ~PolicyAudit() = default;
    virtual void record(std::string_view reason, const Observation &before,
                        std::optional<ChargeLimit> desired, const Observation &after) noexcept = 0;
};
// Owned by machine authority. Callers serialize reconciliation and user commands.
class MaintainChargePolicy {
  public:
    explicit MaintainChargePolicy(PolicyStore &store, PolicyAudit *audit = nullptr)
        : store_(store), audit_(audit) {}
    Observation select(ChargeControl &control, ChargeLimit limit);
    Observation reconcile(ChargeControl &control, std::string_view reason = "scheduled");
    Observation guard(ChargeControl &control, const Telemetry &sample);

  private:
    PolicyStore &store_;
    PolicyAudit *audit_;
    bool unsettled_{false};
    Observation apply(ChargeControl &control, ChargeLimit limit);
    Observation rearm(ChargeControl &control);
    Observation rearm_full(ChargeControl &control);
};
} // namespace manager::application::battery
