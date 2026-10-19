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
    void toggleDashboard();
    [[nodiscard]] bool saveDashboard(const QString &path);
  signals:
    void openManager();
    void restartComponents();

  private:
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
    void activate(QSystemTrayIcon::ActivationReason reason);
    void retranslate();
};
} // namespace manager::presentation
