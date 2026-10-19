#pragma once

#include "application/overview/host_info_port.hpp"

namespace manager::adapters {
class HostInfo final : public application::HostInfoPort {
  public:
    [[nodiscard]] domain::HostPlatform platform() const override;
};
} // namespace manager::adapters
