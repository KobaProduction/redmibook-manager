#pragma once

#include "adapters/windows/firmware/mifs_transport.hpp"
#include "application/performance/performance_control.hpp"

namespace manager::adapters::windows_performance {

using windows_firmware::FirmwareIdentity;
using windows_firmware::FirmwareSupport;
using windows_firmware::MifsTransport;

class PerformanceControl final : public application::performance::PerformanceControl {
  public:
    PerformanceControl(MifsTransport &transport, const FirmwareIdentity &identity);
    [[nodiscard]] application::performance::Status capability() const override;
    [[nodiscard]] application::performance::Observation read() override;
    [[nodiscard]] application::performance::Observation
    request(application::performance::Profile profile) override;

  private:
    MifsTransport &transport_;
    FirmwareSupport support_;
};

} // namespace manager::adapters::windows_performance
