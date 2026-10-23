#include "presentation/tray/tray_client.hpp"
#include <QApplication>
#include <QMessageBox>
#include <QPainter>
#include <QPointer>
#include <utility>

#include "presentation/common/appearance/widget_controls.hpp"
namespace manager::presentation {
namespace {
QIcon tray_icon() {
    return QGuiApplication::windowIcon();
}
} // namespace

TrayClient::TrayClient(Preferences &preferences, application::SessionPort &session)
    : runtime_(session), dashboard_(runtime_) {
    runtime_.set_foreground(false);
    status_.setEnabled(false);
    menu_.addAction(&status_);
    menu_.addSeparator();
    menu_.addAction(&open_);
    menu_.addSeparator();
    menu_.addAction(&restart_);
    menu_.addSeparator();
    menu_.addAction(&close_);
    icon_.setContextMenu(&menu_);
    icon_.setIcon(tray_icon());
    connect(&preferences, &Preferences::languageChanged, this, &TrayClient::retranslate);
    connect(&preferences, &Preferences::changed, this, &TrayClient::retranslate);
    connect(&runtime_, &RuntimeMonitor::changed, this, &TrayClient::retranslate);
    connect(&preferences, &Preferences::errorOccurred, this, [this](const QString &message) {
        icon_.showMessage(tr("Preferences"), message, QSystemTrayIcon::Warning);
    });
    connect(&dashboard_, &TrayDashboard::reconnectRequested, &preferences, &Preferences::reconnect);
    bind_preferences(preferences);
    connect_actions();
    retranslate();
}
void TrayClient::bind_preferences(Preferences &preferences) {
    const auto apply = [this, &preferences] {
        preferences_available_ = preferences.available();
        preference_status_ = preferences.connectionText();
        dashboard_.setPreferenceConnection(preferences_available_, preference_status_);
        retranslate();
        dashboard_.set_rounded(preferences.rounded_corners());
        menu_.setWindowFlag(Qt::FramelessWindowHint);
        menu_.setAttribute(Qt::WA_TranslucentBackground);
        menu_.setStyleSheet(popup_menu_style(preferences.rounded_corners()));
    };
    connect(&preferences, &Preferences::changed, this, apply);
    connect(&preferences, &Preferences::connectionChanged, this, apply);
    apply();
}

void TrayClient::retranslate() {
    open_.setText(tr("Open RedmiBook Manager"));
    restart_.setText(tr("Restart background components"));
    close_.setText(tr("Close tray"));
    icon_.setToolTip(QStringLiteral("RedmiBook Manager %1").arg(QStringLiteral(MANAGER_APP_VERSION)));
    status_.setText(preferences_available_ ? runtime_.status_text() : preference_status_);
    status_.setCheckable(true);
    status_.setChecked(preferences_available_ && runtime_.reply().connected &&
                       runtime_.reply().daemon.connected);
    dashboard_.retranslate();
}

bool TrayClient::show() {
    if (!QSystemTrayIcon::isSystemTrayAvailable()) {
        return false;
    }
    icon_.show();
    return true;
}

void TrayClient::connect_actions() {
    connect(&dashboard_, &TrayDashboard::visibilityChanged, this,
            [this](bool visible) { runtime_.set_foreground(visible); });
    connect(&menu_, &QMenu::aboutToShow, &runtime_, &RuntimeMonitor::refresh);
    connect(&dashboard_, &TrayDashboard::visibilityChanged, this, &TrayClient::dashboardVisibilityChanged);
    connect(&open_, &QAction::triggered, this, &TrayClient::openManager);
    connect(&restart_, &QAction::triggered, this, &TrayClient::restartComponents);
    connect(&close_, &QAction::triggered, qApp, &QApplication::quit);
    connect(&dashboard_, &TrayDashboard::openManager, this, &TrayClient::openManager);
    connect(&dashboard_, &TrayDashboard::profileRequested, this, &TrayClient::profileRequested);
    connect(&dashboard_, &TrayDashboard::refreshRequested, this, &TrayClient::refreshRequested);
    connect(&dashboard_, &TrayDashboard::presetRequested, this, &TrayClient::presetRequested);
    connect(&dashboard_, &TrayDashboard::errorOccurred, this, [this](const QString &message) {
        dashboard_.hide();
        auto *dialog = new QMessageBox(QMessageBox::Critical, tr("Error"), message, QMessageBox::Ok);
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        dialog->open();
    });
    connect(&icon_, &QSystemTrayIcon::activated, this, &TrayClient::activate);
}

void TrayClient::activate(QSystemTrayIcon::ActivationReason reason) {
    if (reason == QSystemTrayIcon::Trigger) {
        toggleDashboard();

    } else if (reason == QSystemTrayIcon::Context) {
        dashboard_.hide();
    }
}

void TrayClient::toggleDashboard() {
    dashboard_.toggle(icon_.geometry());
    if (dashboard_.isVisible())
        emit dashboardOpened();
}

void TrayClient::openDashboard() {
    if (!dashboard_.isVisible())
        toggleDashboard();
}

void TrayClient::setDeviceState(TrayDeviceState state) {
    dashboard_.setDeviceState(std::move(state));
}
bool TrayClient::saveDashboard(const QString &path) {
    return dashboard_.isVisible() && dashboard_.grab().save(path);
}
} // namespace manager::presentation
