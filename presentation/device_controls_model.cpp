#include "presentation/device_controls_model.hpp"
#include <QVariantMap>
#include <QtConcurrentRun>
#include <stdexcept>
#include <utility>

namespace manager::presentation {
DeviceControlsModel::DeviceControlsModel(
    std::shared_ptr<application::microphone::ControlPort> microphone,
    std::shared_ptr<application::display::RefreshPort> refresh, Preferences &preferences,
    std::shared_ptr<application::battery::ChargeControl> battery,
    std::shared_ptr<application::performance::PerformanceControl> performance,
    std::shared_ptr<application::feedback::Presenter> feedback,
    std::shared_ptr<application::microphone::FirmwareStatePort> firmware)
    : microphone_(std::move(microphone)), refresh_(std::move(refresh)), battery_(std::move(battery)),
      performance_(std::move(performance)), feedback_(std::move(feedback)), firmware_(std::move(firmware)),
      preferences_(preferences) {
    if (!microphone_ || !refresh_) {
        throw std::invalid_argument("Device controls require owned ports");
    }
    deadline_.setSingleShot(true);
    deadline_.setInterval(10000);
    connect(&deadline_, &QTimer::timeout, this, &DeviceControlsModel::expireTask);
    connect(&pending_, &QFutureWatcher<TaskResult>::finished, this, &DeviceControlsModel::completeTask);
    connect(&preferences, &Preferences::languageChanged, this, &DeviceControlsModel::changed);
    connect(&preferences, &Preferences::languageChanged, this, &DeviceControlsModel::batteryTelemetryChanged);
    connect(&preferences, &Preferences::changed, this, &DeviceControlsModel::changed);
}
bool DeviceControlsModel::microphoneAvailable() const {
    return microphone_state_.status == application::microphone::Status::Ready &&
           microphone_state_.muted.has_value();
}
bool DeviceControlsModel::microphoneMuted() const {
    return microphone_state_.muted.value_or(false);
}
QString DeviceControlsModel::microphoneStatus() const {
    using application::microphone::Status;
    switch (microphone_state_.status) {
    case Status::NotImplemented:
        return tr("Not implemented on this platform");
    case Status::Denied:
        return tr("Microphone access denied");
    case Status::Failed:
        return tr("Microphone operation failed");
    case Status::Unavailable:
        return tr("Default communications microphone unavailable");
    case Status::Ready:
        break;
    }
    if (!microphone_state_.muted) {
        return tr("Microphone state unknown");
    }
    return microphoneMuted() ? tr("Microphone muted") : tr("Microphone active");
}
QVariantList DeviceControlsModel::screens() const {
    QVariantList result;
    for (const auto &screen : displays_.displays) {
        QVariantList rates;
        for (const auto hz : screen.available_hz) {
            rates.push_back(hz);
        }
        result.push_back(QVariantMap{{QStringLiteral("displayId"), QString::fromStdString(screen.id)},
                                     {QStringLiteral("name"), QString::fromStdString(screen.name)},
                                     {QStringLiteral("currentHz"), screen.current_hz},
                                     {QStringLiteral("rates"), rates}});
    }
    return result;
}
QString DeviceControlsModel::operationStatus() const {
    switch (outcome_) {
    case Outcome::TimedOut:
        return tr("The operation exceeded its deadline. The native call may still finish; its result is "
                  "unconfirmed. Refresh when available.");
    case Outcome::Unknown:
        return tr("The native operation outcome is unknown. Its cancellation is not confirmed.");
    case Outcome::Failed:
        return tr("Change failed or could not be confirmed; refresh status before retrying");
    case Outcome::Running:
    case Outcome::Confirmed:
    case Outcome::None:
        return {};
    }
    return {};
}
void DeviceControlsModel::startTask(std::function<TaskResult()> task, bool command) {
    if (busy_) {
        return;
    }
    busy_ = true;
    command_pending_ = command;
    outcome_ = Outcome::Running;
    elapsed_.start();
    deadline_.start();
    pending_.setFuture(QtConcurrent::run(std::move(task)));
    emit changed();
}

void DeviceControlsModel::expireTask() {
    if (!busy_ || outcome_ == Outcome::TimedOut) {
        return;
    }
    outcome_ = Outcome::TimedOut;
    invalidateState();
    emit changed();
    emit errorOccurred(operationStatus());
}

void DeviceControlsModel::completeTask() {
    deadline_.stop();
    if (elapsed_.elapsed() >= deadline_.interval()) {
        expireTask();
    }
    busy_ = false;
    if (outcome_ != Outcome::TimedOut) {
        try {
            applyTaskResult(pending_.result());
        } catch (...) {
            invalidateState();
            outcome_ = Outcome::Failed;
            emit errorOccurred(operationStatus());
        }
    }
    emit changed();
    if (command_pending_) {
        command_pending_ = false;
        emit stateRefreshRequested();
    }
}

void DeviceControlsModel::refresh() {
    startTask([microphone = microphone_, refresh = refresh_, battery = battery_, performance = performance_] {
        return readSnapshot(microphone, refresh, battery, performance);
    });
}

void DeviceControlsModel::beginRefresh() {
    refreshing_ = true;
    emit changed();
}
void DeviceControlsModel::finishRefresh(application::overview::RefreshResult result) {
    refreshing_ = false;
    emit changed();
    switch (result) {
    case application::overview::RefreshResult::Completed:
        return;
    case application::overview::RefreshResult::Failed:
        emit errorOccurred(tr("Could not refresh device state."));
        return;
    case application::overview::RefreshResult::TimedOut:
        emit errorOccurred(tr("Device state refresh timed out. Native calls may still finish."));
        return;
    case application::overview::RefreshResult::Unavailable:
        emit errorOccurred(tr("Session runtime unavailable"));
        return;
    }
}
void DeviceControlsModel::setMicrophoneMuted(bool muted) {
    if (!preferences_.available() || !microphoneControllable())
        return;
    startTask(
        [firmware = firmware_, muted] {
            TaskResult result;
            result.firmware = firmware->request(muted);
            const auto &state = *result.firmware;
            using application::microphone::FirmwareStateStatus;
            result.outcome = state.status == FirmwareStateStatus::OutcomeUnknown ? Outcome::Unknown
                             : state.status == FirmwareStateStatus::Ready && state.desired_muted == muted
                                 ? Outcome::Confirmed
                                 : Outcome::Failed;
            return result;
        },
        true);
}
void DeviceControlsModel::setRefresh(const QString &id, int hz) {
    if (hz < 2 || hz > 1000) {
        return;
    }
    startTask([refresh = refresh_, display = id.toStdString(),
               hz] { return changeRefresh(refresh, display, static_cast<unsigned int>(hz)); },
              true);
}
} // namespace manager::presentation
