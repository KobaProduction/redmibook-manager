#include "application/display/refresh_selection.hpp"
#include "presentation/device_controls_model.hpp"
#include <QCoreApplication>
#include <algorithm>
namespace manager::presentation {
namespace {
template <class Status> QString support_error(Status status) {
    if (status == Status::NotImplemented) {
        return QCoreApplication::translate("manager::presentation::DeviceControlsModel",
                                           "Not implemented on this platform");
    }
    if (status == Status::UnsupportedHardware) {
        return QCoreApplication::translate("manager::presentation::DeviceControlsModel",
                                           "This hardware is not supported");
    }
    if (status == Status::UnverifiedFirmware) {
        return QCoreApplication::translate("manager::presentation::DeviceControlsModel",
                                           "This firmware has not been qualified");
    }
    return {};
}
template <class Status> QString request_error(Status status) {
    if (status == Status::PermissionDenied) {
        return QCoreApplication::translate("manager::presentation::DeviceControlsModel",
                                           "The core cannot access the firmware");
    }
    if (status == Status::Unavailable) {
        return QCoreApplication::translate("manager::presentation::DeviceControlsModel",
                                           "A compatible core is unavailable");
    }
    if (status == Status::Failed) {
        return QCoreApplication::translate("manager::presentation::DeviceControlsModel",
                                           "The hardware request failed");
    }
    if (status == Status::InvalidRequest) {
        return QCoreApplication::translate("manager::presentation::DeviceControlsModel",
                                           "The requested value is not supported");
    }
    return {};
}
template <class Status> QString hardware_error(Status status) {
    const auto unsupported = support_error(status);
    if (!unsupported.isEmpty()) {
        return unsupported;
    }
    const auto failed = request_error(status);
    if (!failed.isEmpty()) {
        return failed;
    }
    if (status == Status::OutcomeUnknown || status == Status::AppliedUnconfirmed) {
        return QCoreApplication::translate("manager::presentation::DeviceControlsModel",
                                           "The firmware setting could not be confirmed");
    }
    return {};
}
QVariantList rate_values(const std::vector<unsigned int> &rates) {
    QVariantList result;
    for (const auto rate : rates) {
        result.push_back(rate);
    }
    return result;
}
} // namespace
bool DeviceControlsModel::batteryAvailable() const {
    using application::battery::Status;
    return battery_ && battery_capability_.status == Status::Available && battery_state_.limit &&
           (battery_state_.status == Status::Available || battery_state_.status == Status::Confirmed);
}
bool DeviceControlsModel::batteryProtectionEnabled() const {
    return battery_state_.limit == application::battery::ChargeLimit::Protection80;
}
QString DeviceControlsModel::batteryStatus() const {
    if (!battery_) {
        return tr("Not implemented on this platform");
    }
    const auto capability = hardware_error(battery_capability_.status);
    if (!capability.isEmpty()) {
        return capability;
    }
    const auto error = hardware_error(battery_state_.status);
    if (!error.isEmpty()) {
        return error;
    }
    if (!battery_state_.limit) {
        return tr("Battery policy is unknown");
    }
    return batteryProtectionEnabled() ? tr("80% charge protection is enabled")
                                      : tr("Full charging is enabled");
}
bool DeviceControlsModel::profileAvailable() const {
    using application::performance::Status;
    return performance_ && performance_capability_ == Status::Available && profileIndex() >= 0 &&
           (performance_state_.status == Status::Available || performance_state_.status == Status::Confirmed);
}
int DeviceControlsModel::profileIndex() const {
    if (!performance_state_.profile) {
        return -1;
    }
    const int value = static_cast<int>(*performance_state_.profile);
    return value >= 1 && value <= 3 ? value - 1 : -1;
}
QString DeviceControlsModel::profileStatus() const {
    if (!performance_) {
        return tr("Not implemented on this platform");
    }
    const auto capability = hardware_error(performance_capability_);
    if (!capability.isEmpty()) {
        return capability;
    }
    const auto error = hardware_error(performance_state_.status);
    if (!error.isEmpty()) {
        return error;
    }
    switch (profileIndex()) {
    case 0:
        return tr("Balanced");
    case 1:
        return tr("Quiet");
    case 2:
        return tr("Turbo");
    default:
        return tr("Performance profile is unknown");
    }
}
const application::display::Display *
DeviceControlsModel::builtin(const application::display::Snapshot &snapshot) {
    if (snapshot.status != application::display::Status::Ready) {
        return nullptr;
    }
    const application::display::Display *result = nullptr;
    for (const auto &display : snapshot.displays) {
        if (!display.internal_panel) {
            continue;
        }
        if (result) {
            return nullptr;
        }
        result = &display;
    }
    return result;
}
QVariantMap DeviceControlsModel::builtinDisplay() const {
    const auto *display = builtin(displays_);
    if (!display) {
        return {};
    }
    const auto selected = application::display::selected_refresh_rates(display->available_hz,
                                                                       preferences_.refresh_cycle_rates());
    return {{QStringLiteral("displayId"), QString::fromStdString(display->id)},
            {QStringLiteral("name"), QString::fromStdString(display->name)},
            {QStringLiteral("currentHz"), display->current_hz},
            {QStringLiteral("rates"), rate_values(display->available_hz)},
            {QStringLiteral("selectedRates"), rate_values(selected)}};
}
void DeviceControlsModel::acceptState(const application::overview::DeviceState &state) {
    microphone_state_ = state.microphone;
    displays_ = state.displays;
    battery_state_ = state.battery;
    battery_capability_ = state.battery_capability;
    performance_state_ = state.performance;
    performance_capability_ = state.performance_capability;
    emit changed();
}
void DeviceControlsModel::invalidateState() {
    microphone_state_ = {};
    displays_ = {};
    battery_state_ = {};
    battery_capability_ = {};
    performance_state_ = {};
    performance_capability_ = application::performance::Status::Unavailable;
}
void DeviceControlsModel::applyTaskResult(const TaskResult &result) {
    if (result.microphone) {
        microphone_state_ = *result.microphone;
    }
    if (result.displays) {
        displays_ = *result.displays;
    }
    if (result.battery) {
        battery_state_ = *result.battery;
    }
    if (result.battery_capability) {
        battery_capability_ = *result.battery_capability;
    }
    if (result.performance) {
        performance_state_ = *result.performance;
    }
    if (result.performance_capability) {
        performance_capability_ = *result.performance_capability;
    }
    outcome_ = result.outcome;
}
DeviceControlsModel::TaskResult
DeviceControlsModel::readSnapshot(std::shared_ptr<application::microphone::ControlPort> microphone,
                                  std::shared_ptr<application::display::RefreshPort> refresh,
                                  std::shared_ptr<application::battery::ChargeControl> battery,
                                  std::shared_ptr<application::performance::PerformanceControl> performance) {
    TaskResult result{microphone->read(), refresh->read(), Outcome::None};
    if (battery) {
        result.battery = battery->read();
        result.battery_capability = battery->capability();
    }
    if (performance) {
        result.performance = performance->read();
        result.performance_capability = performance->capability();
    }
    return result;
}
DeviceControlsModel::TaskResult
DeviceControlsModel::changeRefresh(std::shared_ptr<application::display::RefreshPort> refresh,
                                   const std::string &id, unsigned int hz) {
    TaskResult result;
    result.displays = refresh->read();
    result.outcome = Outcome::Failed;
    const auto *display = builtin(*result.displays);
    if (!display || display->id != id ||
        std::ranges::find(display->available_hz, hz) == display->available_hz.end()) {
        return result;
    }
    const auto changed = refresh->set_refresh(id, hz);
    result.outcome =
        changed.status == application::display::Status::Ready ? Outcome::Confirmed : Outcome::Failed;
    result.displays = refresh->read();
    return result;
}
void DeviceControlsModel::setBatteryProtection(bool enabled) {
    if (!battery_) {
        return;
    }
    startTask(
        [battery = battery_, enabled] {
            const auto limit = enabled ? application::battery::ChargeLimit::Protection80
                                       : application::battery::ChargeLimit::Full;
            const auto value = application::battery::apply_charge_limit(*battery, limit);
            TaskResult result;
            result.battery = value;
            result.outcome = value.status == application::battery::Status::OutcomeUnknown ? Outcome::Unknown
                             : value.status == application::battery::Status::Confirmed    ? Outcome::Confirmed
                                                                                          : Outcome::Failed;
            return result;
        },
        true);
}
void DeviceControlsModel::setPerformanceProfile(int index) {
    if (!performance_ || index < 0 || index > 2) {
        return;
    }
    startTask(
        [performance = performance_, index] {
            const auto value = application::performance::apply_profile(
                *performance, static_cast<application::performance::Profile>(index + 1));
            TaskResult result;
            result.performance = value;
            result.outcome =
                value.status == application::performance::Status::OutcomeUnknown ? Outcome::Unknown
                : value.status == application::performance::Status::Confirmed    ? Outcome::Confirmed
                                                                                 : Outcome::Failed;
            return result;
        },
        true);
}
void DeviceControlsModel::setCycleRate(int hz, bool selected) {
    const auto *display = builtin(displays_);
    if (!display || hz < 2 || hz > 1000) {
        return;
    }
    const auto rate = static_cast<unsigned int>(hz);
    if (std::ranges::find(display->available_hz, rate) == display->available_hz.end()) {
        return;
    }
    auto rates = application::display::selected_refresh_rates(display->available_hz,
                                                              preferences_.refresh_cycle_rates());
    std::erase(rates, rate);
    if (selected) {
        rates.push_back(rate);
    }
    if (application::display::valid_refresh_selection(rates)) {
        preferences_.set_refresh_cycle_rates(rates);
    }
    emit changed();
}
} // namespace manager::presentation
