#pragma once
#include "application/microphone/firmware_state_port.hpp"
#include "contracts/local_api/firmware_protocol.hpp"
#include <algorithm>
#include <array>
namespace manager::adapters::windows {
inline contracts::firmware::Status wire_status(application::microphone::FirmwareStateStatus status) {
    using Input = application::microphone::FirmwareStateStatus;
    using Output = contracts::firmware::Status;
    switch (status) {
    case Input::Ready:
        return Output::Available;
    case Input::NotImplemented:
        return Output::NotImplemented;
    case Input::UnsupportedHardware:
        return Output::UnsupportedHardware;
    case Input::UnverifiedFirmware:
        return Output::UnverifiedFirmware;
    case Input::Denied:
        return Output::PermissionDenied;
    case Input::Unavailable:
        return Output::Unavailable;
    case Input::OutcomeUnknown:
        return Output::OutcomeUnknown;
    case Input::Incompatible:
        return Output::Incompatible;
    default:
        return Output::Failed;
    }
}
inline application::microphone::FirmwareStateStatus microphone_status(contracts::firmware::Status status) {
    using Input = contracts::firmware::Status;
    using Output = application::microphone::FirmwareStateStatus;
    switch (status) {
    case Input::Available:
        return Output::Ready;
    case Input::NotImplemented:
        return Output::NotImplemented;
    case Input::UnsupportedHardware:
        return Output::UnsupportedHardware;
    case Input::UnverifiedFirmware:
        return Output::UnverifiedFirmware;
    case Input::PermissionDenied:
        return Output::Denied;
    case Input::Unavailable:
        return Output::Unavailable;
    case Input::OutcomeUnknown:
        return Output::OutcomeUnknown;
    case Input::Incompatible:
        return Output::Incompatible;
    default:
        return Output::Failed;
    }
}
template <class Status>
constexpr std::array<Status, 11> hardware_statuses{Status::Available,
                                                   Status::NotImplemented,
                                                   Status::UnsupportedHardware,
                                                   Status::UnverifiedFirmware,
                                                   Status::PermissionDenied,
                                                   Status::Unavailable,
                                                   Status::Failed,
                                                   Status::InvalidRequest,
                                                   Status::AppliedUnconfirmed,
                                                   Status::Confirmed,
                                                   Status::OutcomeUnknown};
template <class Status> contracts::firmware::Status wire_status(Status value) {
    const auto &values = hardware_statuses<Status>;
    const auto match = std::ranges::find(values, value);
    return match == values.end() ? contracts::firmware::Status::Failed
                                 : static_cast<contracts::firmware::Status>(match - values.begin());
}
template <class Status> Status application_status(contracts::firmware::Status value) {
    const auto index = static_cast<unsigned>(value);
    return index < hardware_statuses<Status>.size() ? hardware_statuses<Status>[index] : Status::Unavailable;
}
} // namespace manager::adapters::windows
