#pragma once
#include "application/device_profile/profile_port.hpp"
namespace manager::adapters {
class NativeDeviceProfile final : public application::DeviceProfilePort {
  public:
    application::DeviceProfile read() const override;
};
} // namespace manager::adapters
