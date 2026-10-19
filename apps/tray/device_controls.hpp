#pragma once
#include "adapters/local_api/device_state_stream.hpp"
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
    std::shared_ptr<application::performance::PerformanceControl> performance_;
    std::shared_ptr<application::display::RefreshPort> refresh_;
    application::overview::DeviceState state_;
    QFutureWatcher<bool> watcher_;
    QTimer deadline_;
    QElapsedTimer elapsed_;
    bool connected_{false};
    bool busy_{false};
    Feedback feedback_{Feedback::None};
    void project();
    void set_profile(int index);
    void cycle_refresh();
    void run(std::function<bool()> operation);
    void complete();
};
} // namespace manager::tray
