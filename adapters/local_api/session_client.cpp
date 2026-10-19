#include "adapters/local_api/session_client.hpp"
#include "adapters/platform/development_scope.hpp"
#include "adapters/platform/runtime.hpp"
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QGuiApplication>
#include <QProcess>
#include <QStandardPaths>
#include <array>

#include "adapters/local_api/session_codec.hpp"
#include <QIcon>
namespace manager::adapters {
QString session_endpoint() {
    const auto identity = QByteArray::fromStdString(session_identity() + development_scope());
    const auto digest = QCryptographicHash::hash(identity, QCryptographicHash::Sha256).toHex().left(24);
    return QStringLiteral("KobaProduction.Redmibook.Session.") + QString::fromLatin1(digest);
}
QString preferences_directory() {
    const auto scope = development_scope();
    if (!scope.empty()) {
        const auto root = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
        return QDir(root).filePath(QStringLiteral("Koba Production/Redmibook Manager Development/") +
                                   QString::fromStdString(scope));
    }
    return QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
}

void configure_gui_identity() {
    QCoreApplication::setOrganizationName(QStringLiteral("Koba Production"));
    QCoreApplication::setApplicationName(QStringLiteral("Redmibook Manager"));
    QGuiApplication::setApplicationDisplayName(QStringLiteral("RedmiBook Manager"));
    QCoreApplication::setApplicationVersion(QStringLiteral(MANAGER_APP_VERSION));
    QGuiApplication::setWindowIcon(QIcon(QStringLiteral(":/icons/app-icon.ico")));
    register_application_restart();
}

bool launch_component(const QString &name, const QStringList &arguments) {
    if (!development_scope().empty()) {
        return false;
    }
    QProcess process;
    process.setProgram(QDir(QCoreApplication::applicationDirPath()).filePath(name));
    process.setArguments(arguments);
    process.setWorkingDirectory(QStandardPaths::writableLocation(QStandardPaths::TempLocation));
    return process.startDetached();
}

bool ensure_session() {
    QLocalSocket socket;
    socket.connectToServer(session_endpoint());
    if (socket.waitForConnected(100)) {
        return true;
    }
    return launch_component(QStringLiteral("redmibook-session"), {QStringLiteral("--with-tray")});
}

SessionClient::SessionClient(QObject *parent) : QObject(parent) {
    timeout_.setSingleShot(true);
    connect(&timeout_, &QTimer::timeout, this, [this] { complete(QStringLiteral("session-unavailable")); });
    connect(&socket_, &QLocalSocket::connected, this, [this] { socket_.write(request_); });
    connect(&socket_, &QLocalSocket::readyRead, this, &SessionClient::receive);
    connect(&socket_, &QLocalSocket::errorOccurred, this,
            [this] { complete(QStringLiteral("session-unavailable")); });
}

void SessionClient::complete(const QString &status) {
    if (!active_) {
        return;
    }
    active_ = false;
    timeout_.stop();
    socket_.abort();
    const auto result = decode_session_reply(status.toUtf8());
    auto callback = std::move(completion_);
    if (callback) {
        callback(result);
    }
}

void SessionClient::receive() {
    buffer_ += socket_.readAll();
    if (buffer_.size() > 512) {
        complete(QStringLiteral("protocol-error"));
    } else if (buffer_.contains('\n')) {
        complete(QString::fromUtf8(buffer_.split('\n').first()));
    }
}

bool SessionClient::request(application::SessionCommand command,
                            std::function<void(application::SessionReply)> completion) {
    if (active_) {
        return false;
    }
    active_ = true;
    buffer_.clear();
    completion_ = std::move(completion);
    const std::array<QByteArray, 5> commands{"status", "diagnostic", "restart", "shutdown", "open-tray"};
    request_ = QByteArray("RMB/1 ") + commands.at(static_cast<std::size_t>(command)) + '\n';
    timeout_.start(3000);
    socket_.connectToServer(session_endpoint());
    return true;
}
} // namespace manager::adapters
