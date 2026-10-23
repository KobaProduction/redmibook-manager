#include "apps/tray/tray_operations.hpp"
#include "adapters/local_api/session_client.hpp"
#include "adapters/storage/ui_preferences/store.hpp"
#include "presentation/tray/tray_client.hpp"
#include <QDir>
#include <QLocalSocket>
#include <QLockFile>
#include <QMessageBox>
#include <QTemporaryDir>
#include <QTimer>

namespace manager::tray {
bool close_existing() {
    QLocalSocket socket;
    socket.connectToServer(adapters::session_endpoint() + QStringLiteral(".tray"));
    if (!socket.waitForConnected(500)) {
        return false;
    }
    socket.write("close\n");
    return socket.waitForBytesWritten(500);
}

Instance::Instance(const QString &directory)
    : handover_(
          QDir(directory).filePath(adapters::session_endpoint() + QStringLiteral(".tray.handover.lock"))),
      instance_(QDir(directory).filePath(adapters::session_endpoint() + QStringLiteral(".tray.lock"))) {}

bool Instance::acquire() {
    if (!handover_.tryLock(5000)) {
        return false;
    }
    if (!instance_.tryLock()) {
        close_existing();
        if (!instance_.tryLock(4000)) {
            return false;
        }
    }
    const bool ready = enable_close_endpoint(endpoint_);
    handover_.unlock();
    return ready;
}

bool enable_close_endpoint(QLocalServer &server) {
    const auto name = adapters::session_endpoint() + QStringLiteral(".tray");
    server.setSocketOptions(QLocalServer::UserAccessOption);
    QLocalServer::removeServer(name);
    QObject::connect(&server, &QLocalServer::newConnection, &server, [&server] {
        auto *socket = server.nextPendingConnection();
        socket->setReadBufferSize(32);
        QObject::connect(socket, &QLocalSocket::readyRead, socket, [socket] {
            if (!socket->canReadLine()) {
                return;
            }
            if (socket->readLine() == "close\n") {
                QCoreApplication::quit();
            }
            socket->disconnectFromServer();
        });
        QObject::connect(socket, &QLocalSocket::disconnected, socket, &QObject::deleteLater);
        QTimer::singleShot(1000, socket, &QObject::deleteLater);
    });
    return server.listen(name);
}

int dashboard_smoke(QApplication &app, const QString &screenshot) {
    QTemporaryDir directory;
    adapters::UiPreferenceStore store(directory.path());
    presentation::Preferences preferences(store);
    preferences.set_language(QStringLiteral("ru"));
    preferences.set_theme(QStringLiteral("dark"));
    adapters::SessionClient session;
    presentation::TrayClient tray(preferences, session);
    tray.setDeviceState({.profileIndex = 0,
                         .profileAvailable = true,
                         .refreshHz = 120,
                         .refreshAvailable = true,
                         .microphoneAvailable = true,
                         .microphoneMuted = false,
                         .status = {},
                         .refreshRates = {60, 120}});
    tray.toggleDashboard();
    QTimer::singleShot(
        400, &tray, [&tray, screenshot] { QCoreApplication::exit(tray.saveDashboard(screenshot) ? 0 : 1); });
    return app.exec();
}
} // namespace manager::tray
