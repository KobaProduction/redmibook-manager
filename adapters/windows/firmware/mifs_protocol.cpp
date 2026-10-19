#include "adapters/windows/firmware/mifs_transport.hpp"

#include <utility>

namespace manager::adapters::windows_firmware {
namespace {

bool reading(MifsCommand command) {
    return command == MifsCommand::ReadCharge || command == MifsCommand::ReadPerformance;
}

bool battery(MifsCommand command) {
    return command == MifsCommand::ReadCharge || command == MifsCommand::Protect80 ||
           command == MifsCommand::FullCharge;
}

std::uint8_t profile_value(MifsCommand command) {
    switch (command) {
    case MifsCommand::Balanced:
        return 1;
    case MifsCommand::Quiet:
        return 2;
    case MifsCommand::Turbo:
        return 3;
    case MifsCommand::FullSpeed:
        return 4;
    default:
        return 0;
    }
}

} // namespace

FirmwareSupport firmware_support(const FirmwareIdentity &identity) {
    if (identity.manufacturer != "XIAOMI" || identity.board != "TM2309")
        return FirmwareSupport::UnsupportedHardware;
    if (identity.bios != "RMAMT6B0P0B0B" && identity.bios != "RMAMT6B0P0A0A")
        return FirmwareSupport::UnverifiedFirmware;
    return FirmwareSupport::VerifiedProtocol;
}

std::array<std::uint8_t, 32> encode(MifsCommand command) {
    std::array<std::uint8_t, 32> packet{};
    packet[1] = reading(command) ? 0xFA : 0xFB;
    packet[3] = battery(command) ? 0x10 : 0x08;
    packet[4] = battery(command) ? 2 : profile_value(command);
    packet[6] = command == MifsCommand::Protect80 ? 1 : 0;
    return packet;
}

std::uint16_t word(const MifsReply &reply, std::size_t offset) {
    return static_cast<std::uint16_t>(reply.bytes.at(offset) | (reply.bytes.at(offset + 1) << 8));
}

std::uint32_t dword(const MifsReply &reply, std::size_t offset) {
    return static_cast<std::uint32_t>(word(reply, offset)) |
           (static_cast<std::uint32_t>(word(reply, offset + 2)) << 16);
}

MifsReply validate_reply(MifsCommand command, MifsReply reply) {
    if (reply.status != TransportStatus::Success)
        return reply;
    if (word(reply, 0) == 0xE000)
        return {TransportStatus::Unsupported, {}, "Firmware rejected this operation"};
    if (word(reply, 0) != 0x8000)
        return {TransportStatus::Failed, {}, "Unknown firmware response status"};
    if (reading(command) && word(reply, 2) != static_cast<std::uint16_t>(encode(command)[3] << 8))
        return {TransportStatus::Failed, {}, "Firmware GET selector mismatch"};
    return reply;
}

} // namespace manager::adapters::windows_firmware
