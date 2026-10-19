#pragma once
#include "contracts/local_api/firmware_protocol.hpp"
#include <algorithm>
#include <array>
namespace manager::adapters::windows {
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
