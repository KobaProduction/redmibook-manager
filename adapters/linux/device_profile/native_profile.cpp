#include "adapters/device_profile/native_profile.hpp"
namespace manager::adapters {
application::DeviceProfile NativeDeviceProfile::read() const {
    return {.status = application::ProfileReadStatus::NotImplemented};
}
} // namespace manager::adapters
