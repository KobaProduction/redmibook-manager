#include "adapters/local_api/session_server.hpp"
#include "adapters/local_api/session_client.hpp"
#include <QCoreApplication>
#include <QTimer>
#include <QtConcurrentRun>
#include <utility>

#include "adapters/local_api/session_codec.hpp"
namespace manager::adapters {
SessionServer::SessionServer(const application::ReadRuntimeStatus &read,
                             application::DiagnosticFeedbackPort &feedback)
    : read_(read), feedback_(feedback) {
    connect(&server_, &QLocalServer::newConnection, this, &SessionServer::accept);
    connect(&watcher_, &QFutureWatcher<application::DiagnosticReply>::finished, this,
            &SessionServer::completed);
}

SessionServer::~SessionServer() {
    watcher_.waitForFinished();
}

bool SessionServer::listen(const QString &endpoint) {
    const auto name = endpoint.isEmpty() ? session_endpoint() : endpoint;
    server_.setSocketOptions(QLocalServer::UserAccessOption);
    server_.setMaxPendingConnections(8);
    QLocalServer::removeServer(name);
    return server_.listen(name);
}

void SessionServer::accept() {
    while (server_.hasPendingConnections()) {
        auto *socket = server_.nextPendingConnection();
        socket->setReadBufferSize(128);
        connect(socket, &QLocalSocket::readyRead, this, [this, socket] { handle(socket); });
        connect(socket, &QLocalSocket::disconnected, socket, &QObject::deleteLater);
        QTimer::singleShot(4000, socket, [socket] {
            socket->abort();
            socket->deleteLater();
        });
    }
}

void SessionServer::respond(QLocalSocket *socket, const application::DiagnosticReply &reply) {
    socket->write(encode_session_reply(reply, static_cast<std::uint32_t>(QCoreApplication::applicationPid()),
                                       {MANAGER_APP_VERSION, MANAGER_BUILD_ID}));
    socket->disconnectFromServer();
}

void SessionServer::probe(QLocalSocket *socket, bool feedback) {
    if (pending_.size() >= 8) {
        socket->disconnectFromServer();
        return;
    }
    pending_.push_back(socket);
    show_feedback_ = show_feedback_ || feedback;
    if (probe_active_) {
        return;
    }
    probe_active_ = true;
    watcher_.setFuture(QtConcurrent::run([this] { return read_.execute(); }));
}

void SessionServer::completed() {
    const auto result = watcher_.result();
    last_reply_ = result;
    probe_active_ = false;
    const auto waiting = std::exchange(pending_, {});
    const bool feedback = std::exchange(show_feedback_, false);
    for (const auto &socket : waiting) {
        if (socket) {
            respond(socket.data(), result);
        }
    }
    if (feedback) {
        feedback_.show_result(result);
    }
}

void SessionServer::handle(QLocalSocket *socket) {
    if (!socket->canReadLine()) {
        return;
    }
    const auto command = socket->readLine(128).trimmed();
    socket->disconnect(this);
    if (command == "RMB/1 status" || command == "RMB/1 diagnostic") {
        probe(socket, command.endsWith("diagnostic"));
    } else if (command == "RMB/1 open-tray") {
        respond(socket, last_reply_);
        emit openTray();
    } else if (command == "RMB/1 shutdown" || command == "RMB/1 restart") {
        stop_session(socket, command.endsWith("restart"));
    } else {
        socket->disconnectFromServer();
    }
}
void SessionServer::stop_session(QLocalSocket *socket, bool restart) {
    respond(socket, last_reply_);
    QTimer::singleShot(100, this, [this, restart] {
        server_.close();
        if (restart) {
            emit restartRequested();
        }
        QCoreApplication::quit();
    });
}
} // namespace manager::adapters
