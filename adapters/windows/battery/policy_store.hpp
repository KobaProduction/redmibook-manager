#pragma once
#include "application/battery/charge_policy.hpp"
#include <filesystem>
namespace manager::adapters::windows_battery {
application::battery::Policy load_policy(const std::filesystem::path &directory);
class PolicyStore final : public application::battery::PolicyStore {
  public:
    application::battery::Policy load() override;
    bool save(application::battery::ChargeLimit limit) override;
    bool mark_unsettled(bool value) override;
};
class PolicyLog final : public application::battery::PolicyAudit {
  public:
    void record(std::string_view reason, const application::battery::Observation &before,
                std::optional<application::battery::ChargeLimit> desired,
                const application::battery::Observation &after) noexcept override;
    const std::string &last_record() const {
        return last_;
    }

  private:
    std::string last_;
};
// Elevated installer only: LocalService + SYSTEM + Administrators, no user writes.
bool provision_policy_directory();
} // namespace manager::adapters::windows_battery
