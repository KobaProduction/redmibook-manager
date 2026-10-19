#pragma once

#include "application/performance/performance_control.hpp"

namespace manager::adapters::linux_performance {

class PerformanceControl final : public application::performance::PerformanceControl {
  public:
    [[nodiscard]] application::performance::Status capability() const override {
        return application::performance::Status::NotImplemented;
    }
    [[nodiscard]] application::performance::Observation read() override {
        return {application::performance::Status::NotImplemented,
                {},
                "Linux performance observation is not implemented"};
    }
    [[nodiscard]] application::performance::Observation request(application::performance::Profile) override {
        return {application::performance::Status::NotImplemented,
                {},
                "Linux performance control is not implemented"};
    }
};

} // namespace manager::adapters::linux_performance
