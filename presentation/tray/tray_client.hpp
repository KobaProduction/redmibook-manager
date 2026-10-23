#pragma once
#include "application/runtime/session_port.hpp"
#include "presentation/common/preferences.hpp"
#include "presentation/tray/dashboard.hpp"
#include <QAction>
#include <QMenu>
#include <QObject>
#include <QSystemTrayIcon>

namespace manager::presentation {
class TrayClient final : public QObject {
    Q_OBJECT
  public:
    TrayClient(Preferences &preferences, application::SessionPort &session);
    [[nodiscard]] bool show();
    void activate(QSystemTrayIcon::ActivationReason reason);
    void toggleDashboard();
    void openDashboard();
    void setDeviceState(TrayDeviceState state);
    [[nodiscard]] bool saveDashboard(const QString &path);
  signals:
    void openManager();
    void restartComponents();
    void profileRequested(int index);
    void refreshRequested(unsigned int hz);
    void presetRequested(const QString &preset, const QString &button);
    void dashboardOpened();
    void dashboardVisibilityChanged(bool visible);

  private:
    bool preferences_available_{true};
    QString preference_status_;
    RuntimeMonitor runtime_;
    TrayDashboard dashboard_;
    QSystemTrayIcon icon_;
    QMenu menu_;
    QAction status_;
    QAction open_;
    QAction restart_;
    QAction close_;
    void connect_actions();

    void bind_preferences(Preferences &preferences);
    void retranslate();
};
} // namespace manager::presentation
