#pragma once
#include "application/device_profile/profile_port.hpp"
namespace manager::adapters {
class DemoDeviceProfile final : public application::DeviceProfilePort {
  public:
    application::DeviceProfile read() const override {
        return {.evidence = application::ProfileEvidence::Demo,
                .model = "Redmi Book Pro 16 (2024)",
                .bios_vendor = "DEMO UEFI",
                .bios_version = "DEMO-0.0",
                .controller = "DEMO-EC",
                .firmware_chip = "DEMO-SPI"};
    }
};
} // namespace manager::adapters
