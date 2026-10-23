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
    auto unsupported = support_error(status);
    if (!unsupported.isEmpty()) {
        return unsupported;
    }
    auto failed = request_error(status);
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
    auto capability = hardware_error(battery_capability_.status);
    if (!capability.isEmpty()) {
        return capability;
    }
    auto error = hardware_error(battery_state_.status);
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
    auto capability = hardware_error(performance_capability_);
    if (!capability.isEmpty()) {
        return capability;
    }
    auto error = hardware_error(performance_state_.status);
    if (!error.isEmpty()) {
        return error;
    }
    switch (profileIndex()) {
    case 0:
        return tr("Balanced");
    case 1:
        return tr("Quiet");
    case 2:
        return tr("Performance");
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
    if (battery_telemetry_ != state.battery_telemetry) {
        battery_telemetry_ = state.battery_telemetry;
        emit batteryTelemetryChanged();
    }
    // A pre-command session snapshot must not overwrite the in-flight control state.
    if (busy_)
        return;
    if (microphone_firmware_ == state.microphone_firmware && microphone_state_ == state.microphone &&
        displays_ == state.displays && battery_state_ == state.battery &&
        battery_capability_ == state.battery_capability && performance_state_ == state.performance &&
        performance_capability_ == state.performance_capability)
        return;
    microphone_state_ = state.microphone;
    microphone_firmware_ = state.microphone_firmware;
    displays_ = state.displays;
    battery_state_ = state.battery;
    battery_capability_ = state.battery_capability;
    performance_state_ = state.performance;
    performance_capability_ = state.performance_capability;
    update_supported_features();
    emit changed();
}
void DeviceControlsModel::update_supported_features() {
    battery_supported_ =
        battery_supported_ || battery_capability_.status == application::battery::Status::Available;
    profile_supported_ =
        profile_supported_ || performance_capability_ == application::performance::Status::Available;
    display_supported_ =
        display_supported_ || (displays_.status == application::display::Status::Ready && builtin(displays_));
}
void DeviceControlsModel::invalidateState() {
    battery_telemetry_ = {};
    emit batteryTelemetryChanged();
    microphone_state_ = {};
    microphone_firmware_ = {};
    displays_ = {};
    battery_state_ = {};
    battery_capability_ = {};
    performance_state_ = {};
    performance_capability_ = application::performance::Status::Unavailable;
}
void DeviceControlsModel::apply_microphone_observations(const TaskResult &result) {
    if (result.firmware)
        microphone_firmware_ = *result.firmware;
    if (result.microphone)
        microphone_state_ = *result.microphone;
}
void DeviceControlsModel::apply_observations(const TaskResult &result) {
    apply_microphone_observations(result);
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
    update_supported_features();
}
void DeviceControlsModel::present_feedback(const TaskResult &result) {
    if (!command_pending_ || outcome_ != Outcome::Confirmed || !feedback_)
        return;

    using application::feedback::Event;
    using application::feedback::Kind;
    if (result.performance && result.performance->profile)
        (void)feedback_->present(
            Event{Kind::Performance, static_cast<unsigned int>(*result.performance->profile)});
    if (result.microphone && result.microphone->muted)
        (void)feedback_->present(Event{Kind::Microphone, *result.microphone->muted ? 1U : 0U});
    if (result.displays) {
        if (const auto *panel = builtin(*result.displays))
            (void)feedback_->present(Event{Kind::Refresh, panel->current_hz});
    }
}
void DeviceControlsModel::applyTaskResult(const TaskResult &result) {
    apply_observations(result);
    outcome_ = result.outcome;
    present_feedback(result);
    if (outcome_ == Outcome::Failed || outcome_ == Outcome::Unknown)
        emit errorOccurred(operationStatus());
    if (result.reset_settings && outcome_ == Outcome::Confirmed)
        emit settingsResetReady();
}
DeviceControlsModel::TaskResult
DeviceControlsModel::prepareReset(const std::shared_ptr<application::battery::ChargeControl> &battery) {
    TaskResult result;
    result.reset_settings = true;
    result.outcome = Outcome::Confirmed;
    if (!battery)
        return result;
    using application::battery::Status;
    const auto capability = battery->capability();
    if (capability.status == Status::NotImplemented || capability.status == Status::UnsupportedHardware ||
        capability.status == Status::UnverifiedFirmware)
        return result;
    const auto value =
        application::battery::apply_charge_limit(*battery, application::battery::ChargeLimit::Full);
    result.battery = value;
    result.outcome =
        value.status == Status::OutcomeUnknown ? Outcome::Unknown
        : value.status == Status::Confirmed && value.limit == application::battery::ChargeLimit::Full
            ? Outcome::Confirmed
            : Outcome::Failed;
    return result;
}
void DeviceControlsModel::prepareSettingsReset() {
    if (!preferences_.available()) {
        emit errorOccurred(preferences_.connectionText());
        return;
    }
    startTask([battery = battery_] { return prepareReset(battery); }, true);
}
DeviceControlsModel::TaskResult DeviceControlsModel::readSnapshot(
    const std::shared_ptr<application::microphone::ControlPort> &microphone,
    const std::shared_ptr<application::display::RefreshPort> &refresh,
    const std::shared_ptr<application::battery::ChargeControl> &battery,
    const std::shared_ptr<application::performance::PerformanceControl> &performance) {
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
DeviceControlsModel::changeRefresh(const std::shared_ptr<application::display::RefreshPort> &refresh,
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
    result.displays = refresh->read();
    const auto *observed = builtin(*result.displays);
    result.outcome = changed.status == application::display::Status::Ready && changed.observed_hz == hz &&
                             observed && observed->id == id && observed->current_hz == hz
                         ? Outcome::Confirmed
                         : Outcome::Failed;
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
        if (!preferences_.set_refresh_cycle_rates(rates))
            emit errorOccurred(tr("Could not save preferences."));
    } else {
        emit errorOccurred(tr("Select at least one supported refresh rate."));
    }
    emit changed();
}
} // namespace manager::presentation
