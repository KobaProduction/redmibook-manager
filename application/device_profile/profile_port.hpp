#pragma once
#include <cstdint>
#include <string>
namespace manager::application {
enum class ProfileEvidence : std::uint8_t { Demo, Unknown, Verified, Observed };
enum class ProfileReadStatus : std::uint8_t { Available, NotImplemented, Failed };
struct DeviceProfile {
    ProfileEvidence evidence{ProfileEvidence::Unknown};
    std::string model{};
    std::string bios_vendor{};
    std::string bios_version{};
    std::string controller{};
    std::string firmware_chip{};
    std::string manufacturer{};
    std::string bios_date{};
    std::string board_model{};
    ProfileReadStatus status{ProfileReadStatus::NotImplemented};
    std::uint32_t native_error{0};
};
class DeviceProfilePort {
  public:
    virtual ~DeviceProfilePort() = default;
    virtual DeviceProfile read() const = 0;
};
} // namespace manager::application
