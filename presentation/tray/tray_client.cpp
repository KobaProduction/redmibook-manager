#include "presentation/tray/tray_client.hpp"
#include <QApplication>
#include <QPainter>
#include <QPointer>

#include "presentation/common/appearance/widget_controls.hpp"
namespace manager::presentation {
namespace {
QIcon tray_icon() {
    return QGuiApplication::windowIcon();
}
} // namespace

TrayClient::TrayClient(Preferences &preferences, application::SessionPort &session)
    : runtime_(session), dashboard_(runtime_) {
    status_.setEnabled(false);
    menu_.addAction(&status_);
    menu_.addSeparator();
    menu_.addAction(&open_);
    menu_.addSeparator();
    menu_.addAction(&restart_);
    menu_.addSeparator();
    menu_.addAction(&close_);
    icon_.setContextMenu(&menu_);
    connect(&preferences, &Preferences::languageChanged, this, &TrayClient::retranslate);
    connect(&preferences, &Preferences::changed, this, &TrayClient::retranslate);
    connect(&runtime_, &RuntimeMonitor::changed, this, &TrayClient::retranslate);
    bind_preferences(preferences);
    connect_actions();
    retranslate();
}
void TrayClient::bind_preferences(Preferences &preferences) {
    const auto apply = [this, &preferences] {
        dashboard_.set_rounded(preferences.rounded_corners());
        menu_.setWindowFlag(Qt::FramelessWindowHint);
        menu_.setAttribute(Qt::WA_TranslucentBackground);
        menu_.setStyleSheet(popup_menu_style(preferences.rounded_corners()));
    };
    connect(&preferences, &Preferences::changed, this, apply);
    apply();
}

void TrayClient::retranslate() {
    open_.setText(tr("Open RedmiBook Manager"));
    restart_.setText(tr("Restart background components"));
    close_.setText(tr("Close tray"));
    icon_.setToolTip(
        tr("RedmiBook Manager %1 — background controls").arg(QStringLiteral(MANAGER_APP_VERSION)));
    status_.setText(runtime_.status_text());
    status_.setCheckable(true);
    status_.setChecked(runtime_.reply().connected && runtime_.reply().daemon.connected);
    icon_.setToolTip(icon_.toolTip() + QLatin1Char('\n') + status_.text());
    dashboard_.retranslate();
    icon_.setIcon(tray_icon());
}

bool TrayClient::show() {
    if (!QSystemTrayIcon::isSystemTrayAvailable()) {
        return false;
    }
    icon_.show();
    return true;
}

void TrayClient::connect_actions() {
    connect(&open_, &QAction::triggered, this, &TrayClient::openManager);
    connect(&restart_, &QAction::triggered, this, &TrayClient::restartComponents);
    connect(&close_, &QAction::triggered, qApp, &QApplication::quit);
    connect(&dashboard_, &TrayDashboard::openManager, this, &TrayClient::openManager);
    connect(&icon_, &QSystemTrayIcon::activated, this, &TrayClient::activate);
}

void TrayClient::activate(QSystemTrayIcon::ActivationReason reason) {
    if (reason == QSystemTrayIcon::Trigger) {
        toggleDashboard();
    } else if (reason == QSystemTrayIcon::DoubleClick) {
        emit openManager();
    } else if (reason == QSystemTrayIcon::Context) {
        dashboard_.hide();
    }
}

void TrayClient::toggleDashboard() {
    dashboard_.toggle(icon_.geometry());
}
bool TrayClient::saveDashboard(const QString &path) {
    return dashboard_.isVisible() && dashboard_.grab().save(path);
}
} // namespace manager::presentation
