#pragma once
#include "adapters/local_api/device_state_stream.hpp"
#include "adapters/platform/session_controls.hpp"
#include "application/microphone/change_source.hpp"
#include <QElapsedTimer>
#include <QFutureWatcher>
#include <QObject>
#include <QTimer>
#include <memory>
#include <mutex>

namespace manager::session {
class DeviceStatePublisher final : public QObject {
  public:
    DeviceStatePublisher();
    ~DeviceStatePublisher() override;
    void refresh();

  private:
    struct Delivery {
        std::mutex mutex;
        DeviceStatePublisher *target{};
    };
    std::shared_ptr<Delivery> delivery_;
    adapters::DeviceStateServer server_;
    std::shared_ptr<application::microphone::ControlPort> microphone_;
    std::shared_ptr<application::display::RefreshPort> display_;
    std::shared_ptr<application::battery::ChargeControl> battery_;
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
    void publish(application::overview::DeviceState state);
    void watch_displays();
    void configure_jobs();
};
} // namespace manager::session
