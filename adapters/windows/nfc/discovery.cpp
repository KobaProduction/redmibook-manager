#include "adapters/windows/nfc/io.hpp"
#include <algorithm>
#include <array>
#include <bcrypt.h>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <setupapi.h>

namespace manager::adapters::nfc::windows {
namespace {
bool ntag_device(HDEVINFO devices, SP_DEVINFO_DATA &device) {
    std::array<wchar_t, 512> value{};
    if (!SetupDiGetDeviceRegistryPropertyW(devices, &device, SPDRP_HARDWAREID, nullptr,
                                           reinterpret_cast<PBYTE>(value.data()),
                                           static_cast<DWORD>(sizeof(value)), nullptr))
        return false;
    return std::wstring_view(value.data()).starts_with(L"ACPI\\VEN_NTAG&DEV_5332") ||
           std::wstring_view(value.data()) == L"ACPI\\NTAG5332";
}
std::vector<unsigned char> driver_bytes() {
    std::array<wchar_t, MAX_PATH> path{};
    if (GetSystemDirectoryW(path.data(), static_cast<UINT>(path.size())) == 0)
        return {};
    std::ifstream stream(std::filesystem::path(path.data()) / "drivers" / "NTAG5Driver.sys",
                         std::ios::binary);
    return {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
}
bool calculate_hash(BCRYPT_ALG_HANDLE algorithm, const std::vector<unsigned char> &bytes,
                    std::array<unsigned char, 32> &digest) {
    BCRYPT_HASH_HANDLE hash{};
    auto result = BCryptCreateHash(algorithm, &hash, nullptr, 0, nullptr, 0, 0);
    if (result >= 0)
        result = BCryptHashData(hash, const_cast<PUCHAR>(bytes.data()), static_cast<ULONG>(bytes.size()), 0);
    if (result >= 0)
        result = BCryptFinishHash(hash, digest.data(), static_cast<ULONG>(digest.size()), 0);
    if (hash)
        BCryptDestroyHash(hash);
    return result >= 0;
}
bool driver_hash_matches(const std::vector<unsigned char> &bytes) {
    if (bytes.empty() || bytes.size() > 1048576)
        return false;
    BCRYPT_ALG_HANDLE algorithm{};
    if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0)
        return false;
    std::array<unsigned char, 32> digest{};
    const auto success = calculate_hash(algorithm, bytes, digest);
    BCryptCloseAlgorithmProvider(algorithm, 0);
    constexpr std::array<unsigned char, 32> qualified{
        0xea, 0xc8, 0xe8, 0xbe, 0x0d, 0xe5, 0x60, 0x11, 0x9e, 0x7a, 0x98, 0x62, 0xde, 0x7d, 0x34, 0x3b,
        0xf7, 0xb9, 0x55, 0x5a, 0xa2, 0x7c, 0x32, 0x2e, 0x89, 0xbc, 0x47, 0x3e, 0x85, 0x2e, 0xf1, 0x80};
    return success && digest == qualified;
}
} // namespace
bool present() {
    const auto devices = SetupDiGetClassDevsW(nullptr, L"ACPI", nullptr, DIGCF_ALLCLASSES | DIGCF_PRESENT);
    if (devices == INVALID_HANDLE_VALUE)
        return false;
    SP_DEVINFO_DATA device{};
    device.cbSize = sizeof(device);
    bool found{};
    for (DWORD index = 0; SetupDiEnumDeviceInfo(devices, index, &device); ++index)
        if (ntag_device(devices, device)) {
            found = true;
            break;
        }
    SetupDiDestroyDeviceInfoList(devices);
    return found;
}
bool supported_driver() {
    return driver_hash_matches(driver_bytes());
}
} // namespace manager::adapters::nfc::windows
