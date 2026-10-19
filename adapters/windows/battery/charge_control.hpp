#pragma once

#include "adapters/windows/firmware/mifs_transport.hpp"
#include "application/battery/charge_control.hpp"

#include <string>

namespace manager::adapters::windows_battery {

using windows_firmware::FirmwareIdentity;
using windows_firmware::FirmwareSupport;
using windows_firmware::MifsTransport;

class ChargeControl final : public application::battery::ChargeControl {
  public:
    ChargeControl(MifsTransport &transport, const FirmwareIdentity &identity);
    [[nodiscard]] application::battery::Capability capability() const override;
    [[nodiscard]] application::battery::Observation read() override;
    [[nodiscard]] application::battery::Observation request(application::battery::ChargeLimit limit) override;

  private:
    MifsTransport &transport_;
    FirmwareSupport support_;
};

} // namespace manager::adapters::windows_battery
