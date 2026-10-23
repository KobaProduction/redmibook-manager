#pragma once
#include "adapters/integration/preset_executor.hpp"
#include "adapters/local_api/device_state_stream.hpp"
#include "adapters/local_api/feedback_channel.hpp"
#include "application/display/refresh_port.hpp"
#include "application/performance/performance_control.hpp"
#include "presentation/tray/tray_client.hpp"
#include <QElapsedTimer>
#include <QFutureWatcher>
#include <QTimer>
#include <functional>
#include <memory>

namespace manager::tray {
// Composition for the widget-only tray; the session stream remains the state authority.
class DeviceControls final : public QObject {
    Q_OBJECT
  public:
    DeviceControls(presentation::TrayClient &tray, presentation::Preferences &preferences);
    void start();

  private:
    enum class Feedback : std::uint8_t { None, Applying, TimedOut, Unconfirmed };
    presentation::TrayClient &tray_;
    presentation::Preferences &preferences_;
    adapters::DeviceStateClient state_client_;
    adapters::FeedbackClient feedback_client_;
    application::feedback::Event pending_feedback_{application::feedback::Kind::Performance, 1};
    std::shared_ptr<application::performance::PerformanceControl> performance_;
    std::shared_ptr<application::display::RefreshPort> refresh_;
    std::shared_ptr<adapters::PresetExecutor> preset_executor_;
    bool pending_preset_{false};
    std::shared_ptr<application::presets::Result> preset_result_;
    application::overview::DeviceState state_;
    QFutureWatcher<bool> watcher_;
    QTimer deadline_;
    QTimer battery_lease_;
    QElapsedTimer elapsed_;
    bool profile_supported_{false};
    bool refresh_supported_{false};
    void preserve_support(presentation::TrayDeviceState &view);
    bool connected_{false};
    bool busy_{false};
    Feedback feedback_{Feedback::None};
    void project();
    void watch_visibility();
    void wire_state();
    void set_profile(int index);
    void cycle_refresh(unsigned int hz);
    void run(std::function<bool()> operation, application::feedback::Event feedback, bool preset = false);
    void apply_preset(const QString &preset, const QString &button);
    void complete();
};
} // namespace manager::tray
