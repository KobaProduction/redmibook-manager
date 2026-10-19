#include "adapters/device_profile/smbios.hpp"
#include <string>
namespace manager::adapters {
namespace {
using Bytes = std::span<const std::uint8_t>;
constexpr std::size_t raw_header_size = 8;
constexpr std::size_t structure_header_size = 4;

std::string display_string(Bytes bytes) {
    std::string result;
    for (const auto value : bytes) {
        // Preserve UTF-8 firmware strings, replacing ASCII control bytes for display.
        result += value >= 32 && value != 127 ? static_cast<char>(value) : '?';
    }
    return result;
}

std::string string_at(Bytes strings, std::uint8_t index) {
    if (index == 0) {
        return {};
    }
    std::size_t offset = 0;
    for (unsigned current = 1; offset < strings.size(); ++current) {
        auto end = offset;
        while (end < strings.size() && strings[end] != 0) {
            ++end;
        }
        if (current == index) {
            return display_string(strings.subspan(offset, end - offset));
        }
        offset = end + 1;
    }
    return {};
}

void read_bios(application::DeviceProfile &profile, Bytes fields, Bytes strings) {
    if (fields.size() < 9) {
        return;
    }
    profile.bios_vendor = string_at(strings, fields[4]);
    profile.bios_version = string_at(strings, fields[5]);
    profile.bios_date = string_at(strings, fields[8]);
    // SMBIOS type 0 offsets 0x16/0x17 are EC firmware major/minor, not chip identity.
    if (fields.size() >= 24 && fields[22] != 0xff && fields[23] != 0xff) {
        profile.controller = std::to_string(fields[22]) + "." + std::to_string(fields[23]);
    }
}

void read_structure(application::DeviceProfile &profile, Bytes fields, Bytes strings) {
    if (fields[0] == 0) {
        read_bios(profile, fields, strings);
    } else if (fields[0] == 1 && fields.size() >= 6) {
        profile.manufacturer = string_at(strings, fields[4]);
        profile.model = string_at(strings, fields[5]);
    } else if (fields[0] == 2 && fields.size() >= 6) {
        profile.board_model = string_at(strings, fields[5]);
    }
}

std::size_t structure_end(Bytes remaining) {
    if (remaining.size() < structure_header_size || remaining[1] < structure_header_size ||
        remaining[1] > remaining.size()) {
        return remaining.size();
    }
    std::size_t end = remaining[1];
    while (end + 1 < remaining.size() && !(remaining[end] == 0 && remaining[end + 1] == 0)) {
        ++end;
    }
    return end + 1 < remaining.size() ? end : remaining.size();
}

bool parse_structures(application::DeviceProfile &profile, Bytes table) {
    std::size_t offset = 0;
    while (offset < table.size()) {
        const auto remaining = table.subspan(offset);
        const auto end = structure_end(remaining);
        if (end == remaining.size()) {
            return false;
        }
        if (remaining[0] == 127) {
            return true;
        }
        const std::size_t length = remaining[1];
        read_structure(profile, remaining.first(length), remaining.subspan(length, end - length));
        offset += end + 2;
    }
    return true;
}
} // namespace

application::DeviceProfile parse_smbios(std::span<const std::uint8_t> raw) {
    application::DeviceProfile profile;
    profile.status = application::ProfileReadStatus::Failed;
    if (raw.size() < raw_header_size) {
        return profile;
    }
    std::uint32_t length = 0;
    for (unsigned index = 0; index < 4; ++index) {
        length |= static_cast<std::uint32_t>(raw[4 + index]) << (8U * index);
    }
    if (length == 0 || length > raw.size() - raw_header_size ||
        !parse_structures(profile, raw.subspan(raw_header_size, length))) {
        return {.status = application::ProfileReadStatus::Failed};
    }
    // An OS-reported identity is an observation, never verification of an EC algorithm.
    profile.evidence = application::ProfileEvidence::Observed;
    profile.status = application::ProfileReadStatus::Available;
    return profile;
}
} // namespace manager::adapters
