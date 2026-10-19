#pragma once

#include "application/shortcuts/vendor_input_port.hpp"
#include <memory>

namespace manager::application {
struct DeviceProfile;
}

namespace manager::adapters {
[[nodiscard]] std::unique_ptr<application::shortcuts::VendorInputPort>
make_native_vendor_input(const application::DeviceProfile &profile);
} // namespace manager::adapters
