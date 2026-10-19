#pragma once
#include <cstdint>
#include <string>
namespace manager::application {
enum class ProfileEvidence : std::uint8_t { Demo, Unknown, Verified };
struct DeviceProfile {
    ProfileEvidence evidence{ProfileEvidence::Unknown};
    std::string model{};
    std::string bios_vendor{};
    std::string bios_version{};
    std::string controller{};
    std::string firmware_chip{};
};
class DeviceProfilePort {
  public:
    virtual ~DeviceProfilePort() = default;
    virtual DeviceProfile read() const = 0;
};
} // namespace manager::application
