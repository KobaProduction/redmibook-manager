#pragma once
#include "application/display/refresh_port.hpp"
#include "application/microphone/control_port.hpp"
#include "application/overview/device_state.hpp"
#include "presentation/common/preferences.hpp"
#include <QElapsedTimer>
#include <QFutureWatcher>
#include <QObject>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>
#include <functional>
#include <memory>
#include <optional>

namespace manager::presentation {
class DeviceControlsModel final : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Created by application composition")
    Q_PROPERTY(bool microphoneAvailable READ microphoneAvailable NOTIFY changed)
    Q_PROPERTY(bool microphoneMuted READ microphoneMuted NOTIFY changed)
    Q_PROPERTY(QString microphoneStatus READ microphoneStatus NOTIFY changed)
    Q_PROPERTY(QVariantList screens READ screens NOTIFY changed)
    Q_PROPERTY(QString operationStatus READ operationStatus NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(bool batteryAvailable READ batteryAvailable NOTIFY changed)
    Q_PROPERTY(bool batteryProtectionEnabled READ batteryProtectionEnabled NOTIFY changed)
    Q_PROPERTY(int batteryPercent READ batteryPercent NOTIFY changed)
    Q_PROPERTY(QString batteryStatus READ batteryStatus NOTIFY changed)
    Q_PROPERTY(bool profileAvailable READ profileAvailable NOTIFY changed)
    Q_PROPERTY(int profileIndex READ profileIndex NOTIFY changed)
    Q_PROPERTY(QString profileStatus READ profileStatus NOTIFY changed)
    Q_PROPERTY(QVariantMap builtinDisplay READ builtinDisplay NOTIFY changed)
  public:
    DeviceControlsModel(std::shared_ptr<application::microphone::ControlPort> microphone,
                        std::shared_ptr<application::display::RefreshPort> refresh, Preferences &preferences,
                        std::shared_ptr<application::battery::ChargeControl> battery = nullptr,
                        std::shared_ptr<application::performance::PerformanceControl> performance = nullptr);
    bool microphoneAvailable() const;
    bool microphoneMuted() const;
    QString microphoneStatus() const;
    QVariantList screens() const;
    QString operationStatus() const;
    bool batteryAvailable() const;
    bool batteryProtectionEnabled() const;
    int batteryPercent() const {
        return -1;
    }
    QString batteryStatus() const;
    bool profileAvailable() const;
    int profileIndex() const;
    QString profileStatus() const;
    QVariantMap builtinDisplay() const;
    void acceptState(const application::overview::DeviceState &state);
    bool busy() const {
        return busy_;
    }
    Q_INVOKABLE void refresh();
    Q_INVOKABLE void setMicrophoneMuted(bool muted);
    Q_INVOKABLE void setRefresh(const QString &id, int hz);
    Q_INVOKABLE void setBatteryProtection(bool enabled);
    Q_INVOKABLE void setPerformanceProfile(int index);
    Q_INVOKABLE void setCycleRate(int hz, bool selected);
  signals:
    void changed();
    void stateRefreshRequested();

  private:
    enum class Outcome : std::uint8_t { None, Running, Confirmed, Failed, TimedOut, Unknown };
    struct TaskResult {
        std::optional<application::microphone::Observation> microphone;
        std::optional<application::display::Snapshot> displays;
        Outcome outcome{Outcome::None};
        std::optional<application::battery::Observation> battery{};
        std::optional<application::battery::Capability> battery_capability{};
        std::optional<application::performance::Observation> performance{};
        std::optional<application::performance::Status> performance_capability{};
    };
    void startTask(std::function<TaskResult()> task, bool command = false);
    void applyTaskResult(const TaskResult &result);
    void invalidateState();
    static TaskResult readSnapshot(std::shared_ptr<application::microphone::ControlPort> microphone,
                                   std::shared_ptr<application::display::RefreshPort> refresh,
                                   std::shared_ptr<application::battery::ChargeControl> battery,
                                   std::shared_ptr<application::performance::PerformanceControl> performance);
    static TaskResult changeRefresh(std::shared_ptr<application::display::RefreshPort> refresh,
                                    const std::string &id, unsigned int hz);
    static const application::display::Display *builtin(const application::display::Snapshot &snapshot);
    void completeTask();
    void expireTask();
    std::shared_ptr<application::microphone::ControlPort> microphone_;
    std::shared_ptr<application::display::RefreshPort> refresh_;
    std::shared_ptr<application::battery::ChargeControl> battery_;
    std::shared_ptr<application::performance::PerformanceControl> performance_;
    Preferences &preferences_;
    application::microphone::Observation microphone_state_;
    application::display::Snapshot displays_;
    application::battery::Observation battery_state_;
    application::battery::Capability battery_capability_;
    application::performance::Observation performance_state_;
    application::performance::Status performance_capability_{application::performance::Status::Unavailable};
    QFutureWatcher<TaskResult> pending_;
    QTimer deadline_;
    QElapsedTimer elapsed_;
    // The deadline bounds UI reporting, not OS execution. Keep the slot occupied
    // until the native call returns; timed-out results never become confirmed.
    Outcome outcome_{Outcome::None};
    bool busy_{false};
    bool command_pending_{false};
};
} // namespace manager::presentation
