#include "adapters/windows/firmware/microphone_state.hpp"
namespace manager::adapters::windows_firmware {
namespace {
using application::microphone::FirmwareStateStatus;
FirmwareStateStatus read_status(TransportStatus status) {
    switch (status) {
    case TransportStatus::Success:
        return FirmwareStateStatus::Ready;
    case TransportStatus::PermissionDenied:
        return FirmwareStateStatus::Denied;
    case TransportStatus::OutcomeUnknown:
        return FirmwareStateStatus::OutcomeUnknown;
    case TransportStatus::Unavailable:
        return FirmwareStateStatus::Unavailable;
    case TransportStatus::Unsupported:
        return FirmwareStateStatus::UnsupportedHardware;
    default:
        return FirmwareStateStatus::Failed;
    }
}
} // namespace
application::microphone::FirmwareState read_microphone_state(MifsTransport &transport,
                                                             const FirmwareIdentity &identity) {
    if (identity.manufacturer != "XIAOMI" || identity.board != "TM2309")
        return {FirmwareStateStatus::UnsupportedHardware, {}, "microphone-firmware-hardware-unqualified"};
    if (identity.bios != "RMAMT6B0P0A0A")
        return {FirmwareStateStatus::UnverifiedFirmware, {}, "microphone-firmware-bios-unqualified"};
    const auto reply =
        validate_reply(MifsCommand::ReadMicrophone, transport.exchange(MifsCommand::ReadMicrophone));
    if (reply.status != TransportStatus::Success)
        return {read_status(reply.status), {}, reply.detail};
    return {FirmwareStateStatus::Ready, dword(reply, 6) == 0,
            "firmware-desired-mute; not-endpoint-or-led-readback"};
}
application::microphone::FirmwareState
request_microphone_state(MifsTransport &transport, const FirmwareIdentity &identity, bool muted) {
    auto before = read_microphone_state(transport, identity);
    if (before.status != FirmwareStateStatus::Ready || !before.desired_muted)
        return before;
    const auto command = muted ? MifsCommand::MuteMicrophone : MifsCommand::UnmuteMicrophone;
    const auto reply = validate_reply(command, transport.exchange(command));
    if (reply.status != TransportStatus::Success)
        return {read_status(reply.status), {}, reply.detail};
    auto after = read_microphone_state(transport, identity);
    if (after.status != FirmwareStateStatus::Ready || !after.desired_muted)
        return {FirmwareStateStatus::OutcomeUnknown, {}, "firmware-microphone-write-unconfirmed"};
    if (after.desired_muted != muted)
        return {FirmwareStateStatus::Failed, after.desired_muted, "firmware-microphone-readback-mismatch"};
    return after;
}
} // namespace manager::adapters::windows_firmware
