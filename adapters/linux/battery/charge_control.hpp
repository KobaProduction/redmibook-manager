#pragma once

#include "application/battery/charge_control.hpp"

namespace manager::adapters::linux_battery {

class ChargeControl final : public application::battery::ChargeControl {
  public:
    [[nodiscard]] application::battery::Capability capability() const override {
        return {application::battery::Status::NotImplemented, {}, "Linux charge control is not implemented"};
    }
    [[nodiscard]] application::battery::Observation read() override {
        return {
            application::battery::Status::NotImplemented, {}, "Linux charge observation is not implemented"};
    }
    [[nodiscard]] application::battery::Observation request(application::battery::ChargeLimit) override {
        return {application::battery::Status::NotImplemented, {}, "Linux charge control is not implemented"};
    }
};

} // namespace manager::adapters::linux_battery
