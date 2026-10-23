#include "adapters/windows/firmware/mifs_transport.hpp"

#include <utility>

namespace manager::adapters::windows_firmware {
namespace {

bool reading(MifsCommand command) {
    return command == MifsCommand::ReadCharge || command == MifsCommand::ReadPerformance ||
           command == MifsCommand::ReadMicrophone;
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
    if (command == MifsCommand::ReadMicrophone || command == MifsCommand::MuteMicrophone ||
        command == MifsCommand::UnmuteMicrophone) {
        packet[1] = reading(command) ? 0xFA : 0xFB;
        packet[3] = 0x0A;
        packet[4] = 5;
        packet[6] = command == MifsCommand::UnmuteMicrophone ? 1 : 0;
        return packet;
    }
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
        return {reading(command) ? TransportStatus::Failed : TransportStatus::OutcomeUnknown,
                {},
                "Unknown firmware response status"};
    if (reading(command) && word(reply, 2) != static_cast<std::uint16_t>(encode(command)[3] << 8))
        return {TransportStatus::Failed, {}, "Firmware GET selector mismatch"};
    if (command == MifsCommand::ReadMicrophone && (word(reply, 4) != 5 || dword(reply, 6) > 1))
        return {TransportStatus::Failed, {}, "Firmware microphone GET payload mismatch"};
    return reply;
}

MifsReply observe_reply(MifsCommand command, MifsReply reply, bool &unknown_write) {
    auto validated = validate_reply(command, std::move(reply));
    if (!reading(command) && validated.status == TransportStatus::OutcomeUnknown)
        unknown_write = true;
    return validated;
}

} // namespace manager::adapters::windows_firmware
