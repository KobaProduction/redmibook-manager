#include "adapters/device_profile/native_profile.hpp"
#include "adapters/device_profile/smbios.hpp"
#include <vector>
#include <windows.h>
namespace manager::adapters {
application::DeviceProfile NativeDeviceProfile::read() const {
    constexpr DWORD provider = 0x52534d42; // 'RSMB', without implementation-defined multichar literals.
    constexpr UINT maximum_size = 1024U * 1024U;
    const auto required = GetSystemFirmwareTable(provider, 0, nullptr, 0);
    if (required == 0 || required > maximum_size) {
        return {.status = application::ProfileReadStatus::Failed, .native_error = GetLastError()};
    }
    std::vector<std::uint8_t> bytes(required);
    const auto written = GetSystemFirmwareTable(provider, 0, bytes.data(), required);
    if (written == 0 || written > required) {
        return {.status = application::ProfileReadStatus::Failed, .native_error = GetLastError()};
    }
    bytes.resize(written);
    return parse_smbios(bytes);
}
} // namespace manager::adapters
