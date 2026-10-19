#include "apps/desktop/manager_window.hpp"
#include "adapters/local_api/session_client.hpp"
#include "adapters/platform/runtime.hpp"
#include "presentation/common/displays.hpp"
#include <QCursor>
#include <QDir>
#include <QLocalSocket>
#include <QQuickWindow>
#include <QTimer>

namespace manager::desktop {
namespace {
void activate_window(QQmlApplicationEngine &engine, const QPoint &anchor) {
    for (auto *object : engine.rootObjects()) {
        if (auto *window = qobject_cast<QQuickWindow *>(object)) {
            presentation::present_window(*window, anchor);
        }
    }
}

void allow_owner_activation(const QString &name) {
    QLockFile owner(QDir(adapters::preferences_directory()).filePath(name + QStringLiteral(".lock")));
    qint64 pid = 0;
    QString host;
    QString app;
    if (owner.getLockInfo(&pid, &host, &app)) {
        adapters::permit_foreground_activation(pid);
    }
}
bool notify_existing(const QString &name) {
    QLocalSocket existing;
    existing.connectToServer(name);
    if (!existing.waitForConnected(100)) {
        return false;
    }
    allow_owner_activation(name);
    const auto point = QCursor::pos();
    existing.write(QStringLiteral("activate %1 %2\n").arg(point.x()).arg(point.y()).toUtf8());
    existing.waitForBytesWritten(500);
    return true;
}
void process_activation(QLocalSocket &socket, QQmlApplicationEngine &engine) {
    if (!socket.canReadLine()) {
        return;
    }
    const auto parts = QString::fromUtf8(socket.readLine()).trimmed().split(QLatin1Char(' '));
    bool x_ok = false;
    bool y_ok = false;
    const int x = parts.value(1).toInt(&x_ok);
    const int y = parts.value(2).toInt(&y_ok);
    if (parts.size() == 3 && parts.first() == QStringLiteral("activate") && x_ok && y_ok) {
        activate_window(engine, {x, y});
    }
    socket.disconnectFromServer();
}
void accept_activation(QLocalServer &server, QQmlApplicationEngine &engine) {
    while (server.hasPendingConnections()) {
        auto *socket = server.nextPendingConnection();
        socket->setReadBufferSize(128);
        QObject::connect(socket, &QLocalSocket::readyRead, &engine,
                         [socket, &engine] { process_activation(*socket, engine); });
        QObject::connect(socket, &QLocalSocket::disconnected, socket, &QObject::deleteLater);
        QTimer::singleShot(1000, socket, &QObject::deleteLater);
    }
}
} // namespace
bool ManagerInstance::acquire(QQmlApplicationEngine &engine) {
    const auto name = adapters::session_endpoint() + QStringLiteral(".manager");
    if (notify_existing(name)) {
        return false;
    }
    lock_ = std::make_unique<QLockFile>(
        QDir(adapters::preferences_directory()).filePath(name + QStringLiteral(".lock")));
    if (!lock_->tryLock()) {
        return false;
    }
    server_.setSocketOptions(QLocalServer::UserAccessOption);
    QLocalServer::removeServer(name);
    connect(&server_, &QLocalServer::newConnection, &engine,
            [this, &engine] { accept_activation(server_, engine); });
    return server_.listen(name);
}
} // namespace manager::desktop
