#pragma once
#include "application/device_profile/profile_port.hpp"
#include <cstdint>
#include <span>
namespace manager::adapters {
// Parses the RawSMBIOSData envelope returned by Windows, without OS or Qt dependencies.
application::DeviceProfile parse_smbios(std::span<const std::uint8_t> raw);
} // namespace manager::adapters
