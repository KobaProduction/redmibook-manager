#include "adapters/local_api/session_client.hpp"
#include "adapters/platform/runtime.hpp"
#include "adapters/storage/ui_preferences/store.hpp"
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
} // namespace

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    app.setQuitOnLastWindowClosed(false);
    app.setStyle(QStringLiteral("Fusion"));
    manager::adapters::configure_gui_identity();
    if (app.arguments().contains(QStringLiteral("--close"))) {
        return manager::tray::close_existing() ? 0 : 1;
    }
    const auto smoke = app.arguments().indexOf(QStringLiteral("--dashboard-smoke"));
    if (smoke > 0 && smoke + 1 < app.arguments().size()) {
        return manager::tray::dashboard_smoke(app, app.arguments().at(smoke + 1));
    }
    manager::adapters::UiPreferenceStore store(manager::adapters::preferences_directory());
    manager::presentation::Preferences preferences(store);
    manager::tray::Instance instance(store.directory());
    if (!instance.acquire()) {
        QMessageBox::warning(
            nullptr, QCoreApplication::translate("TrayClient", "Tray already running"),
            QCoreApplication::translate("TrayClient",
                                        "The previous tray did not close. A second tray was not started."));
        return 1;
    }
    manager::adapters::ensure_session();
    manager::adapters::SessionClient client;
    manager::presentation::TrayClient tray(preferences, client);
    QObject::connect(&tray, &manager::presentation::TrayClient::openManager, &app,
                     [] { manager::adapters::launch_component(QStringLiteral("redmibook-manager")); });
    QObject::connect(&tray, &manager::presentation::TrayClient::restartComponents, &app,
                     [&] { restart(client, app); });
    if (!tray.show()) {
        return 2;
    }
    return app.exec();
}
