#include "apps/session/device_state_publisher.hpp"
#include <QtConcurrent/QtConcurrentRun>

namespace manager::session {
void DeviceStatePublisher::watch_battery() {
    configure_battery_sampling();
    connect(&battery_sampling_, &QTimer::timeout, this, [this] {
        if (!telemetry_busy_)
            refresh_battery();
    });
    connect(&server_, &adapters::DeviceStateServer::batteryWatchChanged, this,
            &DeviceStatePublisher::set_battery_watch);
    connect(&server_, &adapters::DeviceStateServer::batteryRefreshRequested, this,
            &DeviceStatePublisher::request_battery_refresh);
    telemetry_deadline_.setSingleShot(true);
    connect(&telemetry_deadline_, &QTimer::timeout, this, &DeviceStatePublisher::expire_battery);
    connect(&telemetry_pending_, &QFutureWatcher<application::battery::Telemetry>::finished, this,
            &DeviceStatePublisher::complete_battery);
    const auto delivery = delivery_;
    power_watch_ = adapters::watch_power([delivery] {
        const std::lock_guard lock(delivery->mutex);
        if (auto *target = delivery->target)
            QMetaObject::invokeMethod(
                target,
                [target] {
                    target->refresh_battery();
                    target->refresh_firmware();
                },
                Qt::QueuedConnection);
    });
}
void DeviceStatePublisher::refresh_battery() {
    if (telemetry_busy_) {
        telemetry_again_ = true;
        return;
    }
    telemetry_busy_ = true;
    telemetry_expired_ = false;
    telemetry_elapsed_.start();
    telemetry_deadline_.start(10000);
    telemetry_pending_.setFuture(
        QtConcurrent::run(&work_.observations, [port = telemetry_] { return port->read(); }));
}
void DeviceStatePublisher::request_battery_refresh() {
    refresh_battery();
}
void DeviceStatePublisher::set_battery_watch(int intervalMs) {
    battery_watch_ms_ = intervalMs;
    configure_battery_sampling();
    if (intervalMs > 0)
        request_battery_refresh();
}

void DeviceStatePublisher::configure_battery_sampling() {
    const bool available =
        state_.battery_telemetry.status == application::battery::TelemetryStatus::Available;
    const int interval = battery_watch_ms_ > 0 && available ? battery_watch_ms_ : 10000;
    if (battery_sampling_.isActive() && battery_sampling_.interval() == interval)
        return;
    battery_sampling_.setTimerType(interval < 1000 ? Qt::PreciseTimer : Qt::CoarseTimer);
    battery_sampling_.start(interval);
}
void DeviceStatePublisher::expire_battery() {
    telemetry_expired_ = true;
    state_.battery_telemetry = {};
    configure_battery_sampling();
    publish(state_);
    server_.finishRefresh(false);
}
void DeviceStatePublisher::complete_battery() {
    const auto previous = state_.battery_telemetry;
    telemetry_busy_ = false;
    telemetry_deadline_.stop();
    try {
        state_.battery_telemetry = !telemetry_expired_ && telemetry_elapsed_.elapsed() < 10000
                                       ? telemetry_pending_.result()
                                       : application::battery::Telemetry{};
    } catch (...) {
        state_.battery_telemetry = {};
    }
    configure_battery_sampling();
    if (state_.battery_telemetry != previous)
        publish(state_);
    else
        finish_refresh();
    if (telemetry_again_) {
        telemetry_again_ = false;
        refresh_battery();
    }
}
} // namespace manager::session
