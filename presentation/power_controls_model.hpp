#pragma once
#include "application/display/brightness_port.hpp"
#include "application/power/saver_port.hpp"
#include <QElapsedTimer>
#include <QFutureWatcher>
#include <QObject>
#include <QStringList>
#include <QTimer>
#include <QtQml/qqmlregistration.h>
#include <functional>
#include <memory>

namespace manager::presentation {
class PowerControlsModel final : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Created by application composition")
    Q_PROPERTY(int brightness READ brightness NOTIFY changed)
    Q_PROPERTY(bool brightnessAvailable READ brightnessAvailable NOTIFY changed)
    Q_PROPERTY(bool saverAvailable READ saverAvailable NOTIFY changed)
    Q_PROPERTY(bool saverEnabled READ saverEnabled NOTIFY changed)
    Q_PROPERTY(bool saverControllable READ saverControllable NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(QStringList modules READ modules NOTIFY changed)
    Q_PROPERTY(bool canObserve READ canObserve CONSTANT)
  public:
    PowerControlsModel(std::shared_ptr<application::display::BrightnessPort> brightness,
                       std::shared_ptr<application::power::SaverPort> saver);
    ~PowerControlsModel() override;
    int brightness() const {
        return state_.brightness.percent ? static_cast<int>(*state_.brightness.percent) : -1;
    }
    bool brightnessAvailable() const {
        return state_.brightness.status == application::display::BrightnessStatus::Ready;
    }
    bool saverAvailable() const {
        return state_.saver.status == application::power::SaverStatus::Ready;
    }
    bool saverEnabled() const {
        return state_.saver.enabled.value_or(false);
    }
    bool saverControllable() const {
        return saverAvailable() && state_.saver.controllable;
    }
    bool busy() const {
        return busy_ && (command_ || static_cast<bool>(next_command_));
    }
    bool canObserve() const {
        return brightness_ || saver_;
    }
    QStringList modules() const {
        QStringList result;
        if (brightness_supported_)
            result.append(QStringLiteral("brightness"));
        if (saver_supported_)
            result.append(QStringLiteral("energy-saver"));
        return result;
    }
    Q_INVOKABLE void refresh();
    Q_INVOKABLE void setBrightness(int percent);
    Q_INVOKABLE void setSaver(bool enabled);
    void setPreviewState(int percent, bool saver);
  signals:
    void changed();
    void errorOccurred(const QString &message);

  private:
    struct State {
        application::display::Brightness brightness;
        application::power::Saver saver;
        bool saver_command{false};
    };
    void start(std::function<State()> task, bool command = false);
    void readState(bool includeBrightness);
    void complete();
    void configure_reconciliation();
    void notify_state(const State &previous, bool was_command);
    std::shared_ptr<application::display::BrightnessPort> brightness_;
    std::shared_ptr<application::power::SaverPort> saver_;
    State state_;
    QFutureWatcher<State> pending_;
    QTimer deadline_;
    QTimer reconciliation_;
    QElapsedTimer elapsed_;
    bool busy_{false};
    bool expired_{false};
    bool brightness_supported_{false};
    bool saver_supported_{false};
    bool command_{false};
    bool refresh_requested_{false};
    bool full_refresh_requested_{false};
    std::function<State()> next_command_;
    std::unique_ptr<application::power::SaverObservation> observation_;
};
} // namespace manager::presentation
