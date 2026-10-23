#include "presentation/power_controls_model.hpp"
#include <QtConcurrentRun>
#include <tuple>
#include <utility>

namespace manager::presentation {
PowerControlsModel::PowerControlsModel(std::shared_ptr<application::display::BrightnessPort> brightness,
                                       std::shared_ptr<application::power::SaverPort> saver)
    : brightness_(std::move(brightness)), saver_(std::move(saver)) {
    deadline_.setSingleShot(true);
    connect(&deadline_, &QTimer::timeout, this, [this] {
        expired_ = true;
        state_ = {};
        emit changed();
        emit errorOccurred(tr("The power-control request timed out. Its result is unconfirmed."));
    });
    connect(&pending_, &QFutureWatcher<State>::finished, this, &PowerControlsModel::complete);
    if (saver_) {
        observation_ = saver_->observe(
            [this] { QMetaObject::invokeMethod(this, [this] { readState(false); }, Qt::QueuedConnection); });
    }
    configure_reconciliation();
}
void PowerControlsModel::configure_reconciliation() {
    reconciliation_.setParent(this);
    reconciliation_.setObjectName(QStringLiteral("powerReconciliation"));
    reconciliation_.setInterval(10000);
    reconciliation_.setTimerType(Qt::CoarseTimer);
    connect(&reconciliation_, &QTimer::timeout, this, &PowerControlsModel::refresh);
    if (canObserve()) {
        reconciliation_.start();
        QTimer::singleShot(0, this, &PowerControlsModel::refresh);
    }
}
PowerControlsModel::~PowerControlsModel() {
    observation_.reset();
}
void PowerControlsModel::start(std::function<State()> task, bool command) {
    if (busy_) {
        if (command && !busy()) {
            next_command_ = std::move(task);
            emit changed();
        }
        return;
    }
    busy_ = true;
    command_ = command;
    expired_ = false;
    elapsed_.start();
    deadline_.start(10000);
    pending_.setFuture(QtConcurrent::run(std::move(task)));
    if (command_)
        emit changed();
}
void PowerControlsModel::refresh() {
    readState(true);
}
void PowerControlsModel::readState(bool includeBrightness) {
    if (!brightness_ && !saver_)
        return;
    if (busy_) {
        refresh_requested_ = true;
        full_refresh_requested_ = full_refresh_requested_ || includeBrightness;
        return;
    }
    start([brightness = brightness_, saver = saver_, cached = state_.brightness, includeBrightness] {
        return State{includeBrightness && brightness ? brightness->read() : cached,
                     saver ? (includeBrightness ? saver->reconcile() : saver->read())
                           : application::power::Saver{}};
    });
}
void PowerControlsModel::setBrightness(int percent) {
    if (!brightness_ || percent < 0 || percent > 100 || !brightnessAvailable())
        return;
    start(
        [brightness = brightness_, saver = saver_, percent] {
            return State{brightness->set(static_cast<unsigned int>(percent)),
                         saver ? saver->read() : application::power::Saver{}};
        },
        true);
}
void PowerControlsModel::setSaver(bool enabled) {
    if (!saver_ || !saverControllable())
        return;
    start(
        [brightness = state_.brightness, saver = saver_, enabled] {
            const auto result = saver->set(enabled);
            return State{brightness, result, true};
        },
        true);
}
void PowerControlsModel::notify_state(const State &previous, bool was_command) {
    brightness_supported_ = brightness_supported_ || brightnessAvailable();
    saver_supported_ = saver_supported_ || saverControllable();
    const auto values = [](const State &state) {
        return std::tuple{state.brightness.status, state.brightness.percent, state.saver.status,
                          state.saver.enabled, state.saver.controllable};
    };
    if (was_command || values(previous) != values(state_))
        emit changed();
    if (state_.brightness.status == application::display::BrightnessStatus::Unconfirmed)
        emit errorOccurred(tr("The brightness change could not be confirmed. Refresh before retrying."));
    if (state_.saver_command && state_.saver.status != application::power::SaverStatus::Ready)
        emit errorOccurred(tr("Windows did not confirm the energy saver change. Refresh before retrying."));
}
void PowerControlsModel::complete() {
    const bool timed_out = expired_ || elapsed_.elapsed() >= 10000;
    deadline_.stop();
    const auto previous = state_;
    const bool was_command = busy();
    busy_ = false;
    try {
        state_ = timed_out ? State{} : pending_.result();
    } catch (...) {
        state_ = {};
    }
    notify_state(previous, was_command);
    if (next_command_) {
        auto command = std::move(next_command_);
        next_command_ = {};
        start(std::move(command), true);
    } else if (std::exchange(refresh_requested_, false)) {
        readState(std::exchange(full_refresh_requested_, false));
    }
}
void PowerControlsModel::setPreviewState(int percent, bool saver) {
    // Preview composition has no native ports and cannot issue hardware commands.
    if (brightness_ || saver_)
        return;
    state_ = {{application::display::BrightnessStatus::Ready, static_cast<unsigned int>(percent)},
              {application::power::SaverStatus::Ready, saver, true}};
    brightness_supported_ = true;
    saver_supported_ = true;
    emit changed();
}
} // namespace manager::presentation
