#pragma once

#include "domain/system/host_platform.hpp"

namespace manager::application {
class HostInfoPort {
  public:
    virtual ~HostInfoPort() = default;
    [[nodiscard]] virtual domain::HostPlatform platform() const = 0;
};
} // namespace manager::application
