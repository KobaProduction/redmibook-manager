#include "adapters/local_api/preference_channel.hpp"
#include "adapters/storage/ui_preferences/presets_codec.hpp"
#include <QJsonArray>
#include <QJsonDocument>
#include <algorithm>

namespace manager::adapters {
namespace {
constexpr qint64 frame_limit = 131072;
QByteArray frame(QJsonObject object) {
    object.insert("protocol", 1);
    object.insert("version", MANAGER_APP_VERSION);
    object.insert("build", MANAGER_BUILD_ID);
    return QJsonDocument(object).toJson(QJsonDocument::Compact) + '\n';
}
bool compatible(const QJsonObject &object) {
    return object.value("protocol").toInt() == 1 && object.value("version") == MANAGER_APP_VERSION &&
           object.value("build") == MANAGER_BUILD_ID;
}
} // namespace
PreferenceServer::PreferenceServer(UiPreferenceStore &store, QString endpoint)
    : store_(store), endpoint_(std::move(endpoint)) {
    connect(&server_, &QLocalServer::newConnection, this, &PreferenceServer::accept);
    store_.observe_error([this] {
        for (auto *peer : peers_)
            peer->write(frame({{"error", "save-failed"}, {"ok", false}}));
    });
}
PreferenceServer::~PreferenceServer() {
    for (auto *peer : peers_) {
        peer->disconnect(this);
        peer->abort();
    }
    peers_.clear();
    store_.observe({});
    store_.observe_error({});
}
bool PreferenceServer::listen() {
    server_.setSocketOptions(QLocalServer::UserAccessOption);
    server_.setMaxPendingConnections(16);
    QLocalServer::removeServer(endpoint_);
    return server_.listen(endpoint_);
}
void PreferenceServer::accept() {
    while (server_.hasPendingConnections()) {
        auto *socket = server_.nextPendingConnection();
        socket->setReadBufferSize(frame_limit + 1);
        if (peers_.size() >= 16) {
            socket->abort();
            socket->deleteLater();
            continue;
        }
        peers_.insert(socket);
        connect(socket, &QLocalSocket::readyRead, this, [this, socket] { handle(socket); });
        connect(socket, &QLocalSocket::disconnected, this, [this, socket] {
            peers_.remove(socket);
            socket->deleteLater();
        });
        QTimer::singleShot(5000, socket, [socket] {
            if (!socket->property("qualified").toBool())
                socket->abort();
        });
    }
}
void PreferenceServer::send(QLocalSocket *socket, qint64 id, bool ok) {
    if (socket->bytesToWrite() > frame_limit * 2) {
        socket->abort();
        return;
    }
    if (!store_.read()) {
        socket->write(frame({{"id", id}, {"ok", false}, {"error", "invalid-settings"}}));
        return;
    }
    socket->write(
        frame({{"id", id}, {"ok", ok}, {"snapshot", QJsonDocument::fromJson(store_.snapshot()).object()}}));
}
void PreferenceServer::broadcast() {
    const auto peers = peers_;
    for (auto *peer : peers)
        if (peer->property("qualified").toBool())
            send(peer);
}
void PreferenceServer::handle(QLocalSocket *socket) {
    if (!socket->canReadLine() && socket->bytesAvailable() > frame_limit) {
        socket->abort();
        return;
    }
    while (socket->canReadLine()) {
        const auto object = QJsonDocument::fromJson(socket->readLine(frame_limit + 1)).object();
        if (!handle_frame(socket, object))
            return;
    }
}
bool PreferenceServer::handle_frame(QLocalSocket *socket, const QJsonObject &object) {
    if (!compatible(object)) {
        socket->write(frame({{"ok", false}, {"error", "incompatible"}}));
        socket->disconnectFromServer();
        return false;
    }
    const auto id = object.value("id").toInteger();
    if (object.value("op") == "hello") {
        socket->setProperty("qualified", true);
        send(socket, id);
        return true;
    }
    if (!socket->property("qualified").toBool()) {
        socket->abort();
        return false;
    }
    send(socket, id, apply(object));
    return true;
}
bool PreferenceServer::apply(const QJsonObject &object) {
    const auto op = object.value("op").toString();
    const auto current = store_.read();
    const bool destructive = object.value("key") == "trayPresets" || op == "reset";
    if (destructive &&
        (!current || object.value("revision").toInteger(-1) != static_cast<qint64>(current->revision)))
        return false;
    if (op == "reset")
        return store_.reset_defaults();
    if (op != "update")
        return false;
    const auto key = object.value("key").toString();
    const QStringList allowed{
        "language",  "theme",           "osdPosition",       "osdScreen",    "trayAutostart",
        "design",    "autoUpdates",     "roundedCorners",    "osdSound",     "refreshCycleRates",
        "fnSAction", "vendorKeyAction", "batteryIntervalMs", "traySections", "trayPresets",
        "osdEnabled"};
    return allowed.contains(key) &&
           store_.update(key.toStdString(), object.value("value").toString().toStdString());
}
PreferenceClient::PreferenceClient(QString endpoint) : endpoint_(std::move(endpoint)) {
    reconnect_.setSingleShot(true);
    reconnect_.setInterval(1000);
    deadline_.setSingleShot(true);
    deadline_.setInterval(5000);
    connect(&reconnect_, &QTimer::timeout, this, &PreferenceClient::connect_server);
    connect(&deadline_, &QTimer::timeout, this, [this] {
        fail();
        socket_.abort();
    });
    connect(&socket_, &QLocalSocket::connected, this, [this] {
        socket_.write(frame({{"op", "hello"}, {"id", 0}}));
        deadline_.start();
    });
    connect(&socket_, &QLocalSocket::readyRead, this, &PreferenceClient::receive);
    connect(&socket_, &QLocalSocket::disconnected, this, [this] {
        fail();
        schedule_reconnect();
    });
    connect(&socket_, &QLocalSocket::errorOccurred, this, [this] {
        fail();
        schedule_reconnect();
    });
    socket_.setReadBufferSize(frame_limit + 1);
    QTimer::singleShot(0, this, &PreferenceClient::connect_server);
}
void PreferenceClient::set_connection(application::PreferenceConnection state) {
    if (connection_ == state)
        return;
    connection_ = state;
    const auto notify = connection_changed_;
    if (notify)
        notify();
}
void PreferenceClient::reconnect() {
    reconnect_.stop();
    socket_.abort();
    set_connection(application::PreferenceConnection::Connecting);
    reconnect_delay_ = 250;
    connect_server();
}
void PreferenceClient::connect_server() {
    if (socket_.state() == QLocalSocket::UnconnectedState)
        socket_.connectToServer(endpoint_);
}
void PreferenceClient::schedule_reconnect() {
    if (reconnect_.isActive())
        return;
    reconnect_.start(reconnect_delay_);
    reconnect_delay_ = std::min(10000, reconnect_delay_ * 2);
}
PreferenceClient::~PreferenceClient() {
    socket_.disconnect(this);
    socket_.abort();
}
std::optional<application::UiPreferences> PreferenceClient::read() const {
    return cache_;
}
void PreferenceClient::fail() {
    compatible_ = false;
    if (connection_ != application::PreferenceConnection::Incompatible)
        set_connection(application::PreferenceConnection::Unavailable);
    deadline_.stop();
    const bool lost = !pending_.isEmpty();
    pending_.clear();
    if (lost && error_)
        error_();
    if (recovery_)
        recovery_();
}
void PreferenceClient::receive() {
    if (!socket_.canReadLine() && socket_.bytesAvailable() > frame_limit) {
        socket_.abort();
        return;
    }
    while (socket_.canReadLine()) {
        const auto message = QJsonDocument::fromJson(socket_.readLine(frame_limit + 1)).object();
        if (!receive_frame(message))
            return;
    }
}
bool PreferenceClient::receive_frame(const QJsonObject &message) {
    if (!compatible(message)) {
        set_connection(application::PreferenceConnection::Incompatible);
        socket_.abort();
        return false;
    }
    if (message.value("error") == "invalid-settings") {
        fail();
        socket_.abort();
        return false;
    }
    const bool snapshot = accept_snapshot(message);
    pending_.remove(message.value("id").toInteger());
    if (pending_.isEmpty())
        deadline_.stop();
    if ((!message.value("ok").toBool() || !snapshot) && error_)
        error_();
    return true;
}
bool PreferenceClient::accept_snapshot(const QJsonObject &message) {
    const auto snapshot = UiPreferenceStore::decode_snapshot(
        QJsonDocument(message.value("snapshot").toObject()).toJson(QJsonDocument::Compact));
    if (!snapshot || message.contains("error"))
        return false;
    cache_ = snapshot;
    compatible_ = true;
    set_connection(application::PreferenceConnection::Ready);
    reconnect_delay_ = 250;
    const auto notify = changed_;
    if (notify)
        notify();
    return true;
}
bool PreferenceClient::request(QJsonObject message) {
    if (!compatible_ || !cache_ || socket_.state() != QLocalSocket::ConnectedState || pending_.size() >= 16)
        return false;
    const auto id = next_id_++;
    if (message.value("key") == "trayPresets" || message.value("op") == "reset")
        message.insert("revision", static_cast<qint64>(cache_->revision));
    message.insert("id", id);
    const auto bytes = frame(message);
    if (bytes.size() > frame_limit || socket_.write(bytes) < 0)
        return false;
    pending_.insert(id);
    if (!deadline_.isActive())
        deadline_.start();
    return true;
}
bool PreferenceClient::update(std::string_view key, std::string_view value) {
    return request({{"op", "update"},
                    {"key", QString::fromUtf8(key.data(), static_cast<qsizetype>(key.size()))},
                    {"value", QString::fromUtf8(value.data(), static_cast<qsizetype>(value.size()))}});
}
bool PreferenceClient::reset_defaults() {
    return request({{"op", "reset"}});
}
bool PreferenceClient::save_tray_presets(const std::vector<application::presets::Preset> &presets) {
    if (!application::presets::valid(presets))
        return false;
    return update("trayPresets",
                  QJsonDocument(encode_presets(presets)).toJson(QJsonDocument::Compact).toStdString());
}
} // namespace manager::adapters
