#pragma once

#include "application/overview/host_info_port.hpp"

#include <string>

namespace manager::application {
struct Overview {
    std::string greeting;
    domain::HostPlatform platform;
};

class ReadOverview final {
  public:
    explicit ReadOverview(const HostInfoPort &host);
    [[nodiscard]] Overview execute() const;

  private:
    const HostInfoPort &host_;
};
} // namespace manager::application
