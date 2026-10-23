#pragma once
#include "application/display/refresh_port.hpp"
#include "application/feedback/device_feedback.hpp"
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
    Q_PROPERTY(bool microphoneControllable READ microphoneControllable NOTIFY changed)
    Q_PROPERTY(bool microphoneFirmwareMuted READ microphoneFirmwareMuted NOTIFY changed)
    Q_PROPERTY(bool microphoneAvailable READ microphoneAvailable NOTIFY changed)
    Q_PROPERTY(bool microphoneMuted READ microphoneMuted NOTIFY changed)
    Q_PROPERTY(QString microphoneStatus READ microphoneStatus NOTIFY changed)
    Q_PROPERTY(bool displaySupported READ displaySupported NOTIFY changed)
    Q_PROPERTY(QVariantList screens READ screens NOTIFY changed)
    Q_PROPERTY(QString operationStatus READ operationStatus NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(bool refreshing READ refreshing NOTIFY changed)
    Q_PROPERTY(bool batterySupported READ batterySupported NOTIFY changed)
    Q_PROPERTY(bool profileSupported READ profileSupported NOTIFY changed)
    Q_PROPERTY(bool batteryAvailable READ batteryAvailable NOTIFY changed)
    Q_PROPERTY(bool batteryProtectionEnabled READ batteryProtectionEnabled NOTIFY changed)
    Q_PROPERTY(int batteryPercent READ batteryPercent NOTIFY batteryTelemetryChanged)
    Q_PROPERTY(QString batteryStatus READ batteryStatus NOTIFY changed)
    Q_PROPERTY(QVariantMap batteryTelemetry READ batteryTelemetry NOTIFY batteryTelemetryChanged)
    Q_PROPERTY(bool profileAvailable READ profileAvailable NOTIFY changed)
    Q_PROPERTY(int profileIndex READ profileIndex NOTIFY changed)
    Q_PROPERTY(QString profileStatus READ profileStatus NOTIFY changed)
    Q_PROPERTY(QVariantMap builtinDisplay READ builtinDisplay NOTIFY changed)
  public:
    DeviceControlsModel(std::shared_ptr<application::microphone::ControlPort> microphone,
                        std::shared_ptr<application::display::RefreshPort> refresh, Preferences &preferences,
                        std::shared_ptr<application::battery::ChargeControl> battery = nullptr,
                        std::shared_ptr<application::performance::PerformanceControl> performance = nullptr,
                        std::shared_ptr<application::feedback::Presenter> feedback = nullptr,
                        std::shared_ptr<application::microphone::FirmwareStatePort> firmware = nullptr);
    bool microphoneControllable() const {
        return firmware_ &&
               microphone_firmware_.status == application::microphone::FirmwareStateStatus::Ready &&
               microphone_firmware_.desired_muted.has_value();
    }
    bool microphoneFirmwareMuted() const {
        return microphone_firmware_.desired_muted.value_or(false);
    }
    bool microphoneAvailable() const;
    bool microphoneMuted() const;
    QString microphoneStatus() const;
    bool displaySupported() const {
        return display_supported_;
    }
    QVariantList screens() const;
    QString operationStatus() const;
    bool batterySupported() const {
        return battery_supported_;
    }
    bool profileSupported() const {
        return profile_supported_;
    }
    bool batteryAvailable() const;
    bool batteryProtectionEnabled() const;
    int batteryPercent() const {
        return battery_telemetry_.percent.value_or(-1);
    }
    QVariantMap batteryTelemetry() const;
    QString batteryStatus() const;
    bool profileAvailable() const;
    int profileIndex() const;
    QString profileStatus() const;
    QVariantMap builtinDisplay() const;
    void acceptState(const application::overview::DeviceState &state);
    bool busy() const {
        return busy_;
    }
    bool refreshing() const {
        return refreshing_;
    }
    void beginRefresh();
    void finishRefresh(application::overview::RefreshResult result);
    Q_INVOKABLE void refresh();
    Q_INVOKABLE void requestRefresh() {
        emit stateRefreshRequested();
    }
    Q_INVOKABLE void requestBatteryTelemetryRefresh() {
        emit batteryTelemetryRefreshRequested();
    }
    Q_INVOKABLE void watchBatteryTelemetry(int intervalMs) {
        emit batteryWatchRequested(intervalMs);
    }
    Q_INVOKABLE void setMicrophoneMuted(bool muted);
    Q_INVOKABLE void setRefresh(const QString &id, int hz);
    Q_INVOKABLE void setBatteryProtection(bool enabled);
    Q_INVOKABLE void prepareSettingsReset();
    Q_INVOKABLE void setPerformanceProfile(int index);
    Q_INVOKABLE void setCycleRate(int hz, bool selected);
  signals:
    void settingsResetReady();
    void changed();
    void stateRefreshRequested();
    void batteryTelemetryRefreshRequested();
    void batteryWatchRequested(int intervalMs);
    void batteryTelemetryChanged();
    void errorOccurred(const QString &message);

  private:
    QString batteryState() const;
    enum class Outcome : std::uint8_t { None, Running, Confirmed, Failed, TimedOut, Unknown };
    struct TaskResult {
        std::optional<application::microphone::Observation> microphone;
        std::optional<application::display::Snapshot> displays;
        Outcome outcome{Outcome::None};
        std::optional<application::battery::Observation> battery{};
        std::optional<application::battery::Capability> battery_capability{};
        std::optional<application::performance::Observation> performance{};
        std::optional<application::performance::Status> performance_capability{};
        bool reset_settings{false};
        std::optional<application::microphone::FirmwareState> firmware{};
    };
    void startTask(std::function<TaskResult()> task, bool command = false);
    void applyTaskResult(const TaskResult &result);
    void apply_observations(const TaskResult &result);
    void apply_microphone_observations(const TaskResult &result);
    void present_feedback(const TaskResult &result);
    void invalidateState();
    static TaskResult
    readSnapshot(const std::shared_ptr<application::microphone::ControlPort> &microphone,
                 const std::shared_ptr<application::display::RefreshPort> &refresh,
                 const std::shared_ptr<application::battery::ChargeControl> &battery,
                 const std::shared_ptr<application::performance::PerformanceControl> &performance);
    static TaskResult changeRefresh(const std::shared_ptr<application::display::RefreshPort> &refresh,
                                    const std::string &id, unsigned int hz);
    static TaskResult prepareReset(const std::shared_ptr<application::battery::ChargeControl> &battery);
    static const application::display::Display *builtin(const application::display::Snapshot &snapshot);
    void completeTask();
    void expireTask();
    std::shared_ptr<application::microphone::ControlPort> microphone_;
    std::shared_ptr<application::display::RefreshPort> refresh_;
    std::shared_ptr<application::battery::ChargeControl> battery_;
    std::shared_ptr<application::performance::PerformanceControl> performance_;
    std::shared_ptr<application::feedback::Presenter> feedback_;
    std::shared_ptr<application::microphone::FirmwareStatePort> firmware_;
    application::microphone::FirmwareState microphone_firmware_;
    Preferences &preferences_;
    application::microphone::Observation microphone_state_;
    application::display::Snapshot displays_;
    application::battery::Observation battery_state_;
    application::battery::Telemetry battery_telemetry_;
    application::battery::Capability battery_capability_;
    application::performance::Observation performance_state_;
    application::performance::Status performance_capability_{application::performance::Status::Unavailable};
    QFutureWatcher<TaskResult> pending_;
    QTimer deadline_;
    QElapsedTimer elapsed_;
    // The deadline bounds UI reporting, not OS execution. Keep the slot occupied
    // until the native call returns; timed-out results never become confirmed.
    Outcome outcome_{Outcome::None};
    void update_supported_features();
    bool display_supported_{false};
    bool battery_supported_{false};
    bool profile_supported_{false};
    bool busy_{false};
    bool command_pending_{false};
    bool refreshing_{false};
};
} // namespace manager::presentation
