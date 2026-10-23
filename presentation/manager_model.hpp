#pragma once
#include "application/runtime/session_port.hpp"
#include "presentation/common/displays.hpp"
#include "presentation/common/preferences.hpp"
#include "presentation/common/runtime_monitor.hpp"
#include <QObject>
#include <QtQml/qqmlregistration.h>
#include <utility>

namespace manager::presentation {
class ManagerModel final : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Created by application composition")
    Q_PROPERTY(bool roundedCorners READ roundedCorners NOTIFY changed)
    Q_PROPERTY(qreal cornerRadius READ cornerRadius NOTIFY changed)
    Q_PROPERTY(bool designPending READ designPending NOTIFY changed)
    Q_PROPERTY(int languageIndex READ languageIndex NOTIFY changed)
    Q_PROPERTY(int themeIndex READ themeIndex NOTIFY changed)
    Q_PROPERTY(int osdPositionIndex READ osdPositionIndex NOTIFY changed)
    Q_PROPERTY(bool trayAutostart READ trayAutostart NOTIFY changed)
    Q_PROPERTY(int vendorKeyAction READ vendorKeyAction NOTIFY changed)
    Q_PROPERTY(int fnSAction READ fnSAction NOTIFY changed)
    Q_PROPERTY(QString osdScreen READ osdScreen NOTIFY changed)
    Q_PROPERTY(int designIndex READ designIndex NOTIFY changed)
    Q_PROPERTY(bool autoUpdates READ autoUpdates NOTIFY changed)
    Q_PROPERTY(bool osdSound READ osdSound NOTIFY changed)
    Q_PROPERTY(bool osdEnabled READ osdEnabled NOTIFY changed)
    Q_PROPERTY(int batteryIntervalMs READ batteryIntervalMs NOTIFY changed)
    Q_PROPERTY(QVariantList traySections READ traySections NOTIFY changed)
    Q_PROPERTY(QVariantList displays READ displays NOTIFY displaysChanged)
    Q_PROPERTY(bool preferenceError READ preferenceError NOTIFY changed)
    Q_PROPERTY(QString runtimeStatus READ runtimeStatus NOTIFY changed)
    Q_PROPERTY(QString daemonDetails READ daemonDetails NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
  public:
    bool osdEnabled() const {
        return preferences_.osd_enabled();
    }
    Q_INVOKABLE void setOsdEnabled(bool enabled) {
        preference_error_ = !preferences_.set_osd_enabled(enabled);
        emit changed();
    }
    ManagerModel(Preferences &preferences, application::SessionPort &runtime);
    [[nodiscard]] int languageIndex() const;
    [[nodiscard]] int themeIndex() const;
    [[nodiscard]] int osdPositionIndex() const;
    [[nodiscard]] bool trayAutostart() const;
    int vendorKeyAction() const;
    int fnSAction() const;
    QString osdScreen() const;
    int designIndex() const;
    bool designPending() const;
    bool roundedCorners() const;
    qreal cornerRadius() const;
    bool autoUpdates() const;
    bool osdSound() const {
        return preferences_.osd_sound();
    }
    Q_INVOKABLE void setOsdSound(bool enabled) {
        preference_error_ = !preferences_.set_osd_sound(enabled);
        emit changed();
    }
    QVariantList displays() const;
    int batteryIntervalMs() const {
        return preferences_.battery_interval_ms();
    }
    Q_INVOKABLE void setBatteryIntervalMs(int interval) {
        if (!preferences_.set_battery_interval_ms(interval))
            emit errorOccurred(tr("Could not save preferences."));
    }
    QVariantList traySections() const;
    Q_INVOKABLE void setTraySection(const QString &id, bool enabled) {
        if (!preferences_.set_tray_section_enabled(id, enabled))
            emit errorOccurred(tr("Could not save preferences."));
    }
    bool preferenceError() const {
        return preference_error_;
    }
    [[nodiscard]] QString runtimeStatus() const;
    [[nodiscard]] QString daemonDetails() const;
    [[nodiscard]] bool busy() const {
        return busy_ || monitor_.pending();
    }
    Q_INVOKABLE void setLanguage(int index);
    Q_INVOKABLE void setTheme(int index);
    Q_INVOKABLE void setOsdPosition(int index);
    Q_INVOKABLE void setTrayAutostart(bool enabled);
    Q_INVOKABLE void setVendorKeyAction(int action);
    Q_INVOKABLE void setFnSAction(int action);
    Q_INVOKABLE void setOsdScreen(const QString &id);
    Q_INVOKABLE void setDesign(int index);
    Q_INVOKABLE void applyDesign();
    Q_INVOKABLE void resetSettings();
    Q_INVOKABLE void setRoundedCorners(bool enabled);
    Q_INVOKABLE void setAutoUpdates(bool enabled);
    Q_INVOKABLE void diagnostic();
    Q_INVOKABLE void refresh();
    Q_INVOKABLE void openTray();
    Q_INVOKABLE bool takePreferenceError() {
        return std::exchange(preference_error_, false);
    }
  signals:
    void changed();
    void displaysChanged();
    void restartRequested();
    void errorOccurred(const QString &message);

  private:
    void observe_design();
    bool design_restart_pending_{false};
    bool reset_pending_{false};
    bool design_initialized_{false};
    Preferences &preferences_;
    application::SessionPort &runtime_;
    RuntimeMonitor monitor_;
    DisplayCatalog displays_;
    QString applied_design_;
    int pending_design_{0};
    bool busy_{false};
    bool preference_error_{false};
    void request(application::SessionCommand command);
    void finishRequest(const application::SessionReply &reply);
};
} // namespace manager::presentation
