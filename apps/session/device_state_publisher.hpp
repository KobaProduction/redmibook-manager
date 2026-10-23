#pragma once
#include "adapters/local_api/device_state_stream.hpp"
#include "adapters/platform/battery.hpp"
#include "adapters/platform/session_controls.hpp"
#include "application/microphone/change_source.hpp"
#include "application/microphone/firmware_state_port.hpp"
#include "apps/session/work_queues.hpp"
#include <QElapsedTimer>
#include <QFutureWatcher>
#include <QObject>
#include <QTimer>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <utility>

namespace manager::session {
class DeviceStatePublisher final : public QObject {
  public:
    explicit DeviceStatePublisher(WorkQueues &work);
    ~DeviceStatePublisher() override;
    void refresh();
    void setMicrophoneReconciliation(std::function<void()> callback) {
        reconcile_microphone_ = std::move(callback);
    }

  private:
    struct Delivery {
        std::mutex mutex;
        DeviceStatePublisher *target{};
    };
    WorkQueues &work_;
    std::function<void()> reconcile_microphone_;
    std::shared_ptr<application::microphone::FirmwareStatePort> microphone_firmware_;
    std::shared_ptr<Delivery> delivery_;
    adapters::DeviceStateServer server_;
    std::shared_ptr<application::microphone::ControlPort> microphone_;
    std::shared_ptr<application::display::RefreshPort> display_;
    std::shared_ptr<application::battery::ChargeControl> battery_;
    std::shared_ptr<application::battery::TelemetryPort> telemetry_{adapters::make_battery_telemetry()};
    std::unique_ptr<application::battery::PowerSubscription> power_watch_;
    QTimer battery_sampling_;
    QTimer reconciliation_;
    std::optional<application::overview::DeviceState> published_state_;
    int battery_watch_ms_{};
    QTimer telemetry_deadline_;
    QElapsedTimer telemetry_elapsed_;
    QFutureWatcher<application::battery::Telemetry> telemetry_pending_;
    bool telemetry_busy_{false};
    bool telemetry_again_{false};
    bool telemetry_expired_{false};
    std::shared_ptr<application::performance::PerformanceControl> performance_;
    std::unique_ptr<application::microphone::ChangeSubscription> microphone_watch_;
    QFutureWatcher<application::overview::DeviceState> pending_;
    QFutureWatcher<application::overview::DeviceState> firmware_pending_;
    QTimer deadline_;
    QTimer firmware_deadline_;
    QElapsedTimer elapsed_;
    QElapsedTimer firmware_elapsed_;
    bool busy_{false};
    bool again_{false};
    bool expired_{false};
    bool firmware_busy_{false};
    bool firmware_again_{false};
    bool firmware_expired_{false};
    application::overview::DeviceState state_;
    std::uint64_t revision_{};
    void complete();
    void refresh_local();
    void refresh_firmware();
    void complete_firmware();
    void expire_firmware();
    void watch_microphone();
    void publish(application::overview::DeviceState state);
    void watch_displays();
    void configure_jobs();
    void watch_battery();
    void refresh_battery();
    void request_battery_refresh();
    void set_battery_watch(int intervalMs);
    void configure_battery_sampling();
    void finish_refresh();
    void complete_battery();
    void expire_battery();
};
} // namespace manager::session
