#include "apps/session/device_state_publisher.hpp"
#include "adapters/platform/firmware_controls.hpp"
#include "adapters/platform/microphone_watch.hpp"
#include <QGuiApplication>
#include <QScreen>
#include <QtConcurrentRun>
#include <utility>

namespace manager::session {
DeviceStatePublisher::DeviceStatePublisher(WorkQueues &work)
    : work_(work), delivery_(std::make_shared<Delivery>()),
      microphone_(std::make_shared<adapters::NativeMicrophone>()),
      display_(std::make_shared<adapters::NativeRefresh>()), battery_(adapters::make_core_charge_control()),
      performance_(adapters::make_core_performance_control()) {
    microphone_firmware_ = adapters::make_core_microphone_state();
    delivery_->target = this;
    configure_jobs();
    connect(&server_, &adapters::DeviceStateServer::refreshRequested, this, &DeviceStatePublisher::refresh);
    server_.listen();
    watch_microphone();
    watch_displays();
    watch_battery();
    reconciliation_.setInterval(10000);
    reconciliation_.setTimerType(Qt::CoarseTimer);
    connect(&reconciliation_, &QTimer::timeout, this, [this] {
        if (reconcile_microphone_)
            reconcile_microphone_();
        refresh_local();
        refresh_firmware();
    });
    reconciliation_.start();
    refresh();
}
void DeviceStatePublisher::watch_microphone() {
    const auto delivery = delivery_;
    microphone_watch_ = adapters::watch_native_microphone([delivery] {
        const std::lock_guard lock(delivery->mutex);
        if (auto *target = delivery->target)
            QMetaObject::invokeMethod(target, [target] { target->refresh_local(); }, Qt::QueuedConnection);
    });
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
        server_.finishRefresh(false);
    });
    connect(&firmware_deadline_, &QTimer::timeout, this, [this] {
        firmware_expired_ = true;
        state_.microphone_firmware = {};
        state_.battery = {};
        state_.performance = {};
        publish(state_);
        server_.finishRefresh(false);
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
    pending_.setFuture(QtConcurrent::run(&work_.observations, [microphone = microphone_, display = display_] {
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
    firmware_pending_.setFuture(
        QtConcurrent::run(&work_.observations,
                          [battery = battery_, performance = performance_, firmware = microphone_firmware_] {
                              application::overview::DeviceState state;
                              if (firmware)
                                  state.microphone_firmware = firmware->read();
                              state.battery_capability = battery->capability();
                              state.battery = battery->read();
                              state.performance_capability = performance->capability();
                              state.performance = performance->read();
                              return state;
                          }));
}
void DeviceStatePublisher::expire_firmware() {
    firmware_expired_ = true;
    state_.microphone_firmware = {};
    state_.battery = {};
    state_.performance = {};
}
void DeviceStatePublisher::complete_firmware() {
    firmware_deadline_.stop();
    firmware_busy_ = false;
    if (!firmware_expired_ && firmware_elapsed_.elapsed() < 10000) {
        try {
            const auto result = firmware_pending_.result();
            state_.microphone_firmware = result.microphone_firmware;
            state_.battery = result.battery;
            state_.battery_capability = result.battery_capability;
            state_.performance = result.performance;
            state_.performance_capability = result.performance_capability;
        } catch (...) {
            expire_firmware();
        }
    } else {
        expire_firmware();
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
    refresh_battery();
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
            expired_ = true;
            state_.microphone = {};
            state_.displays = {};
        }
    } else {
        expired_ = true;
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
    state.revision = 0;
    if (published_state_ && state == *published_state_) {
        finish_refresh();
        return;
    }
    published_state_ = state;
    state_ = std::move(state);
    state_.revision = ++revision_;
    server_.publish(state_);
    finish_refresh();
}
void DeviceStatePublisher::finish_refresh() {
    if (!busy_ && !firmware_busy_ && !telemetry_busy_ && !again_ && !firmware_again_ && !telemetry_again_)
        server_.finishRefresh(!expired_ && !firmware_expired_ && !telemetry_expired_);
}
} // namespace manager::session
