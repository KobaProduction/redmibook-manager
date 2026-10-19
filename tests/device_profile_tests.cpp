#include "adapters/device_profile/native_profile.hpp"
#include "adapters/device_profile/smbios.hpp"
#include <cstdint>
#include <iostream>
#include <string_view>
#include <vector>
namespace {
using Bytes = std::vector<std::uint8_t>;
Bytes envelope(const Bytes &table) {
    Bytes result{0, 3, 5, 0};
    const auto size = static_cast<std::uint32_t>(table.size());
    for (unsigned index = 0; index < 4; ++index) {
        result.push_back(static_cast<std::uint8_t>((size >> (8U * index)) & 0xffU));
    }
    result.insert(result.end(), table.begin(), table.end());
    return result;
}
Bytes table() {
    Bytes bytes(24, 0);
    bytes[1] = 24;
    bytes[4] = 1;
    bytes[5] = 2;
    bytes[8] = 3;
    bytes[22] = 1;
    bytes[23] = 7;
    const Bytes bios{'V', 0, 'B', 0, 'D', 0, 0};
    bytes.insert(bytes.end(), bios.begin(), bios.end());
    const Bytes model{1, 6, 1, 0, 1, 2, 'X', 0, 'R', 0, 0, 2, 6, 2, 0, 0, 1, 'T', 0, 0, 127, 4, 3, 0, 0, 0};
    bytes.insert(bytes.end(), model.begin(), model.end());
    return bytes;
}
bool valid() {
    const auto profile = manager::adapters::parse_smbios(envelope(table()));
    return profile.status == manager::application::ProfileReadStatus::Available &&
           profile.evidence == manager::application::ProfileEvidence::Observed &&
           profile.manufacturer == "X" && profile.model == "R" && profile.bios_vendor == "V" &&
           profile.bios_version == "B" && profile.bios_date == "D" && profile.controller == "1.7" &&
           profile.board_model == "T" && profile.firmware_chip.empty();
}
bool malformed() {
    const auto good = envelope(table());
    for (std::size_t size = 0; size < good.size(); ++size) {
        const auto truncated = std::span(good).first(size);
        if (manager::adapters::parse_smbios(truncated).status !=
            manager::application::ProfileReadStatus::Failed) {
            return false;
        }
    }
    auto broken = table();
    broken[1] = 3;
    return manager::adapters::parse_smbios(envelope(broken)).status ==
           manager::application::ProfileReadStatus::Failed;
}
bool unavailable_fields() {
    auto bytes = table();
    bytes[4] = 99;
    bytes[22] = 0xff;
    const auto profile = manager::adapters::parse_smbios(envelope(bytes));
    return profile.bios_vendor.empty() && profile.controller.empty() && profile.firmware_chip.empty();
}
} // namespace
int main(int argc, char *argv[]) {
    if (argc == 2 && std::string_view(argv[1]) == "--native") {
        const auto profile = manager::adapters::NativeDeviceProfile{}.read();
        std::cout << "status=" << static_cast<unsigned>(profile.status)
                  << "\nmanufacturer=" << profile.manufacturer << "\nmodel=" << profile.model
                  << "\nbios_vendor=" << profile.bios_vendor << "\nbios_version=" << profile.bios_version
                  << "\nbios_date=" << profile.bios_date << "\nboard_model=" << profile.board_model
                  << "\nec_revision=" << profile.controller << "\nnative_error=" << profile.native_error
                  << '\n';
        return 0;
    }
    return valid() && malformed() && unavailable_fields() ? 0 : 1;
}
