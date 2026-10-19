#include "adapters/windows/performance/performance_control.hpp"

namespace manager::adapters::windows_performance {
namespace {

using application::performance::Observation;
using application::performance::Profile;
using application::performance::Status;
using windows_firmware::MifsCommand;
using windows_firmware::TransportStatus;
using windows_firmware::validate_reply;
using windows_firmware::word;

Status transport_status(TransportStatus status) {
    switch (status) {
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

std::optional<MifsCommand> command_for(Profile profile) {
    switch (profile) {
    case Profile::Balanced:
        return MifsCommand::Balanced;
    case Profile::Quiet:
        return MifsCommand::Quiet;
    case Profile::Turbo:
        return MifsCommand::Turbo;
    case Profile::FullSpeed:
        return {};
    }
    return {};
}

} // namespace

PerformanceControl::PerformanceControl(MifsTransport &transport, const FirmwareIdentity &identity)
    : transport_(transport), support_(firmware_support(identity)) {}

Status PerformanceControl::capability() const {
    if (support_ == FirmwareSupport::UnsupportedHardware)
        return Status::UnsupportedHardware;
    if (support_ == FirmwareSupport::UnverifiedFirmware)
        return Status::UnverifiedFirmware;
    return Status::Available;
}

Observation PerformanceControl::read() {
    if (support_ == FirmwareSupport::UnsupportedHardware)
        return {Status::UnsupportedHardware, {}, "No verified performance protocol for this hardware"};
    const auto reply =
        validate_reply(MifsCommand::ReadPerformance, transport_.exchange(MifsCommand::ReadPerformance));
    if (reply.status != TransportStatus::Success)
        return {transport_status(reply.status), {}, reply.detail};
    const auto value = word(reply, 4);
    if (value < 1 || value > 4)
        return {Status::Failed, {}, "Unknown performance profile; no state inferred"};
    return {Status::Available, static_cast<Profile>(value), "Firmware-reported profile"};
}

Observation PerformanceControl::request(Profile profile) {
    if (capability() != Status::Available)
        return {capability(), {}, "Performance writes require a qualified TM2309 BIOS"};
    const auto command = command_for(profile);
    if (!command)
        return {Status::InvalidRequest, {}, "Profile is not advertised by the TM2309 protocol"};
    const auto reply = validate_reply(*command, transport_.exchange(*command));
    if (reply.status == TransportStatus::OutcomeUnknown)
        return {Status::OutcomeUnknown, {}, reply.detail};
    if (reply.status != TransportStatus::Success)
        return {transport_status(reply.status), {}, reply.detail};
    return {Status::AppliedUnconfirmed, {}, "Firmware accepted request; policy readback required"};
}

} // namespace manager::adapters::windows_performance
