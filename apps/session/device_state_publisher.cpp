#include "apps/session/device_state_publisher.hpp"
#include "adapters/platform/firmware_controls.hpp"
#include "adapters/platform/microphone_watch.hpp"
#include <QGuiApplication>
#include <QScreen>
#include <QtConcurrentRun>
#include <utility>

namespace manager::session {
DeviceStatePublisher::DeviceStatePublisher()
    : delivery_(std::make_shared<Delivery>()), microphone_(std::make_shared<adapters::NativeMicrophone>()),
      display_(std::make_shared<adapters::NativeRefresh>()), battery_(adapters::make_core_charge_control()),
      performance_(adapters::make_core_performance_control()) {
    delivery_->target = this;
    configure_jobs();
    connect(&server_, &adapters::DeviceStateServer::refreshRequested, this, &DeviceStatePublisher::refresh);
    server_.listen();
    const auto delivery = delivery_;
    microphone_watch_ = adapters::watch_native_microphone([delivery] {
        const std::lock_guard lock(delivery->mutex);
        if (auto *target = delivery->target)
            QMetaObject::invokeMethod(target, [target] { target->refresh_local(); }, Qt::QueuedConnection);
    });
    watch_displays();
    refresh();
}
DeviceStatePublisher::~DeviceStatePublisher() {
    const std::lock_guard lock(delivery_->mutex);
    delivery_->target = nullptr;
}
void DeviceStatePublisher::configure_jobs() {
    deadline_.setSingleShot(true);
    firmware_deadline_.setSingleShot(true);
    connect(&deadline_, &QTimer::timeout, this, [this] {
        expired_ = true;
        state_.microphone = {};
        state_.displays = {};
        publish(state_);
    });
    connect(&firmware_deadline_, &QTimer::timeout, this, [this] {
        firmware_expired_ = true;
        state_.battery = {};
        state_.performance = {};
        publish(state_);
    });
    connect(&pending_, &QFutureWatcher<application::overview::DeviceState>::finished, this,
            &DeviceStatePublisher::complete);
    connect(&firmware_pending_, &QFutureWatcher<application::overview::DeviceState>::finished, this,
            &DeviceStatePublisher::complete_firmware);
}
void DeviceStatePublisher::refresh_local() {
    if (busy_) {
        again_ = true;
        return;
    }
    busy_ = true;
    expired_ = false;
    elapsed_.start();
    deadline_.start(10000);
    pending_.setFuture(QtConcurrent::run([microphone = microphone_, display = display_] {
        application::overview::DeviceState state;
        state.microphone = microphone->read();
        state.displays = display->read();
        return state;
    }));
}
void DeviceStatePublisher::refresh_firmware() {
    if (firmware_busy_) {
        firmware_again_ = true;
        return;
    }
    firmware_busy_ = true;
    firmware_expired_ = false;
    firmware_elapsed_.start();
    firmware_deadline_.start(10000);
    firmware_pending_.setFuture(QtConcurrent::run([battery = battery_, performance = performance_] {
        application::overview::DeviceState state;
        state.battery_capability = battery->capability();
        state.battery = battery->read();
        state.performance_capability = performance->capability();
        state.performance = performance->read();
        return state;
    }));
}
void DeviceStatePublisher::complete_firmware() {
    firmware_deadline_.stop();
    firmware_busy_ = false;
    if (!firmware_expired_ && firmware_elapsed_.elapsed() < 10000) {
        try {
            const auto result = firmware_pending_.result();
            state_.battery = result.battery;
            state_.battery_capability = result.battery_capability;
            state_.performance = result.performance;
            state_.performance_capability = result.performance_capability;
        } catch (...) {
            state_.battery = {};
            state_.performance = {};
        }
    } else {
        state_.battery = {};
        state_.performance = {};
    }
    publish(state_);
    if (firmware_again_) {
        firmware_again_ = false;
        refresh_firmware();
    }
}
void DeviceStatePublisher::watch_displays() {
    const auto watch = [this](QScreen *screen) {
        connect(screen, &QScreen::refreshRateChanged, this, &DeviceStatePublisher::refresh_local);
        connect(screen, &QScreen::geometryChanged, this, &DeviceStatePublisher::refresh_local);
    };
    for (auto *screen : QGuiApplication::screens())
        watch(screen);
    connect(qGuiApp, &QGuiApplication::screenAdded, this, [this, watch](QScreen *screen) {
        watch(screen);
        refresh_local();
    });
    connect(qGuiApp, &QGuiApplication::screenRemoved, this, &DeviceStatePublisher::refresh_local);
}
void DeviceStatePublisher::refresh() {
    refresh_local();
    refresh_firmware();
}
void DeviceStatePublisher::complete() {
    deadline_.stop();
    busy_ = false;
    if (!expired_ && elapsed_.elapsed() < 10000) {
        try {
            const auto result = pending_.result();
            state_.microphone = result.microphone;
            state_.displays = result.displays;
        } catch (...) {
            state_.microphone = {};
            state_.displays = {};
        }
    } else {
        state_.microphone = {};
        state_.displays = {};
    }
    publish(state_);
    if (again_) {
        again_ = false;
        refresh_local();
    }
}
void DeviceStatePublisher::publish(application::overview::DeviceState state) {
    state_ = std::move(state);
    state_.revision = ++revision_;
    server_.publish(state_);
}
} // namespace manager::session
