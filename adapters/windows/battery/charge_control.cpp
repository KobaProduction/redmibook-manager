#include "adapters/windows/battery/charge_control.hpp"

namespace manager::adapters::windows_battery {
namespace {

using application::battery::ChargeLimit;
using application::battery::Observation;
using application::battery::Status;
using windows_firmware::dword;
using windows_firmware::MifsCommand;
using windows_firmware::TransportStatus;
using windows_firmware::validate_reply;
using windows_firmware::word;

Status status(TransportStatus transport) {
    switch (transport) {
    case TransportStatus::Success:
        return Status::Available;
    case TransportStatus::PermissionDenied:
        return Status::PermissionDenied;
    case TransportStatus::Unavailable:
        return Status::Unavailable;
    case TransportStatus::Unsupported:
        return Status::UnsupportedHardware;
    case TransportStatus::OutcomeUnknown:
        return Status::OutcomeUnknown;
    case TransportStatus::Failed:
        return Status::Failed;
    }
    return Status::Failed;
}

} // namespace

ChargeControl::ChargeControl(MifsTransport &transport, const FirmwareIdentity &identity)
    : transport_(transport), support_(firmware_support(identity)) {}

application::battery::Capability ChargeControl::capability() const {
    if (support_ == FirmwareSupport::UnsupportedHardware)
        return {Status::UnsupportedHardware, {}, "No verified battery protocol for this hardware"};
    if (support_ == FirmwareSupport::UnverifiedFirmware)
        return {Status::UnverifiedFirmware,
                {},
                "TM2309 BIOS is unqualified; read-only diagnostics are available"};
    return {Status::Available,
            {ChargeLimit::Protection80, ChargeLimit::Full},
            "TM2309 fixed 80% / full-charge protocol"};
}

Observation ChargeControl::read() {
    if (support_ == FirmwareSupport::UnsupportedHardware)
        return {Status::UnsupportedHardware, {}, "No verified battery protocol for this hardware"};
    const auto reply = validate_reply(MifsCommand::ReadCharge, transport_.exchange(MifsCommand::ReadCharge));
    if (reply.status != TransportStatus::Success)
        return {status(reply.status), {}, reply.detail};
    if (word(reply, 4) != 2 || dword(reply, 6) > 1)
        return {Status::Failed, {}, "Unexpected TM2309 charge response; no state inferred"};
    const auto limit = dword(reply, 6) == 1 ? ChargeLimit::Protection80 : ChargeLimit::Full;
    return {Status::Available, limit, "Firmware-reported policy; physical cutoff is unverified"};
}

Observation ChargeControl::request(ChargeLimit limit) {
    const auto supported = capability();
    if (supported.status != Status::Available)
        return {supported.status, {}, supported.detail};
    if (!domain::battery::valid(limit))
        return {Status::InvalidRequest, {}, "Only 80% protection and 100% full charge are verified"};
    const auto command =
        limit == ChargeLimit::Protection80 ? MifsCommand::Protect80 : MifsCommand::FullCharge;
    const auto reply = validate_reply(command, transport_.exchange(command));
    if (reply.status == TransportStatus::OutcomeUnknown)
        return {Status::OutcomeUnknown, {}, reply.detail};
    if (reply.status != TransportStatus::Success)
        return {status(reply.status), {}, reply.detail};
    return {Status::AppliedUnconfirmed, {}, "Firmware accepted request; policy readback required"};
}

} // namespace manager::adapters::windows_battery
