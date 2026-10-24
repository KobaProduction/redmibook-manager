#include "adapters/local_api/preference_channel.hpp"
#include "adapters/local_api/session_client.hpp"
#include "adapters/platform/runtime.hpp"
#include "apps/tray/device_controls.hpp"
#include "apps/tray/tray_operations.hpp"
#include "presentation/common/preferences.hpp"
#include "presentation/tray/tray_client.hpp"
#include <QApplication>
#include <QDir>
#include <QFutureWatcher>
#include <QLockFile>
#include <QMessageBox>
#include <QPointer>
#include <QTimer>
#include <QtConcurrentRun>

namespace {
void restart_session(manager::adapters::SessionClient &client, int remaining) {
    if (remaining == 0) {
        QMessageBox::warning(
            nullptr, QCoreApplication::translate("TrayClient", "Restart failed"),
            QCoreApplication::translate("TrayClient", "The session runtime could not be restarted."));
        return;
    }
    const QPointer<manager::adapters::SessionClient> receiver(&client);
    const bool accepted = client.request(manager::application::SessionCommand::Restart,
                                         [receiver](const manager::application::SessionReply &reply) {
                                             if (receiver && !reply.connected) {
                                                 manager::adapters::ensure_session();
                                             }
                                         });
    if (!accepted) {
        QTimer::singleShot(1000, &client, [receiver, remaining] {
            if (receiver) {
                restart_session(*receiver.data(), remaining - 1);
            }
        });
    }
}
void restart(manager::adapters::SessionClient &client, QObject &owner) {
    auto *watcher = new QFutureWatcher<bool>(&owner);
    QObject::connect(watcher, &QFutureWatcher<bool>::finished, &owner, [watcher, &client] {
        if (watcher->result()) {
            restart_session(client, 5);
        } else {
            QMessageBox::warning(
                nullptr, QCoreApplication::translate("TrayClient", "Restart failed"),
                QCoreApplication::translate("TrayClient", "The daemon could not be restarted."));
        }
        watcher->deleteLater();
    });
    watcher->setFuture(QtConcurrent::run([] { return manager::adapters::restart_machine_runtime(); }));
}
bool acquire_instance(manager::tray::Instance &instance) {
    if (instance.acquire())
        return true;
    QMessageBox::warning(
        nullptr, QCoreApplication::translate("TrayClient", "Tray already running"),
        QCoreApplication::translate("TrayClient",
                                    "The previous tray did not close. A second tray was not started."));
    return false;
}
void connect_tray(manager::presentation::TrayClient &tray, manager::adapters::SessionClient &client,
                  QApplication &app) {
    QObject::connect(&tray, &manager::presentation::TrayClient::openManager, &app,
                     [] { manager::adapters::launch_component(QStringLiteral("redmibook-manager")); });
    QObject::connect(&tray, &manager::presentation::TrayClient::restartComponents, &app,
                     [&client, &app] { restart(client, app); });
}
int present_tray(QApplication &app, manager::presentation::TrayClient &tray) {
    if (!tray.show())
        return 2;
    if (app.arguments().contains(QStringLiteral("--open-dashboard")))
        QTimer::singleShot(0, &tray, &manager::presentation::TrayClient::openDashboard);
    return app.exec();
}
int run_tray(QApplication &app) {
    manager::adapters::PreferenceClient store(manager::adapters::preferences_endpoint());
    manager::presentation::Preferences preferences(store);
    QObject::connect(&preferences, &manager::presentation::Preferences::reconnectRequested, &app,
                     [] { manager::adapters::ensure_session(); });
    manager::tray::Instance instance(manager::adapters::preferences_directory());
    if (!acquire_instance(instance))
        return 1;
    manager::adapters::ensure_session();
    manager::adapters::SessionClient client;
    manager::presentation::TrayClient tray(preferences, client);
    manager::tray::DeviceControls controls(tray, preferences);
    controls.start();
    connect_tray(tray, client, app);
    return present_tray(app, tray);
}
} // namespace

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    app.setQuitOnLastWindowClosed(false);
    app.setStyle(QStringLiteral("Fusion"));
    manager::adapters::configure_gui_identity();
    if (app.arguments().contains(QStringLiteral("--close")))
        return manager::tray::close_existing() ? 0 : 1;
    const auto smoke = app.arguments().indexOf(QStringLiteral("--dashboard-smoke"));
    if (smoke > 0 && smoke + 1 < app.arguments().size())
        return manager::tray::dashboard_smoke(app, app.arguments().at(smoke + 1));
    return run_tray(app);
}
