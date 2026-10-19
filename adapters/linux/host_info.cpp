#include "adapters/platform/host_info.hpp"

namespace manager::adapters {
domain::HostPlatform HostInfo::platform() const {
    return domain::HostPlatform::Linux;
}
} // namespace manager::adapters
