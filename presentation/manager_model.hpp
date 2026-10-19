#pragma once
#include "application/runtime/session_port.hpp"
#include "presentation/common/displays.hpp"
#include "presentation/common/preferences.hpp"
#include "presentation/common/runtime_monitor.hpp"
#include <QObject>
#include <QtQml/qqmlregistration.h>

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
    Q_PROPERTY(int fnSAction READ fnSAction NOTIFY changed)
    Q_PROPERTY(int fnKAction READ fnKAction NOTIFY changed)
    Q_PROPERTY(QString osdScreen READ osdScreen NOTIFY changed)
    Q_PROPERTY(int designIndex READ designIndex NOTIFY changed)
    Q_PROPERTY(bool autoUpdates READ autoUpdates NOTIFY changed)
    Q_PROPERTY(QVariantList displays READ displays NOTIFY displaysChanged)
    Q_PROPERTY(bool preferenceError READ preferenceError NOTIFY changed)
    Q_PROPERTY(QString runtimeStatus READ runtimeStatus NOTIFY changed)
    Q_PROPERTY(QString daemonDetails READ daemonDetails NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
  public:
    ManagerModel(Preferences &preferences, application::SessionPort &runtime);
    [[nodiscard]] int languageIndex() const;
    [[nodiscard]] int themeIndex() const;
    [[nodiscard]] int osdPositionIndex() const;
    [[nodiscard]] bool trayAutostart() const;
    int fnSAction() const;
    int fnKAction() const;
    QString osdScreen() const;
    int designIndex() const;
    bool designPending() const;
    bool roundedCorners() const;
    qreal cornerRadius() const;
    bool autoUpdates() const;
    QVariantList displays() const;
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
    Q_INVOKABLE void setFnSAction(int action);
    Q_INVOKABLE void setFnKAction(int action);
    Q_INVOKABLE void setOsdScreen(const QString &id);
    Q_INVOKABLE void setDesign(int index);
    Q_INVOKABLE void applyDesign();
    Q_INVOKABLE void setRoundedCorners(bool enabled);
    Q_INVOKABLE void setAutoUpdates(bool enabled);
    Q_INVOKABLE void diagnostic();
    Q_INVOKABLE void refresh();
    Q_INVOKABLE void openTray();
  signals:
    void changed();
    void displaysChanged();
    void restartRequested();

  private:
    Preferences &preferences_;
    application::SessionPort &runtime_;
    RuntimeMonitor monitor_;
    DisplayCatalog displays_;
    QString applied_design_;
    int pending_design_{0};
    bool busy_{false};
    bool preference_error_{false};
    void request(application::SessionCommand command);
};
} // namespace manager::presentation
