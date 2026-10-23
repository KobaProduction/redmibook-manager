#include "adapters/local_api/device_state_stream.hpp"
#include "adapters/local_api/device_state_codec.hpp"
#include "adapters/local_api/session_client.hpp"
#include "application/ui_preferences/preferences.hpp"
#include <QVariant>
#include <algorithm>

namespace manager::adapters {
namespace {
QString endpoint() {
    return session_endpoint() + QStringLiteral(".DeviceState.v1");
}
constexpr qint64 frame_limit = 65536;
} // namespace
DeviceStateServer::~DeviceStateServer() {
    // Socket children outlive the timer/vector members unless their callbacks are disconnected.
    QObject::disconnect(&server_, nullptr, this, nullptr);
    for (const auto &socket : clients_)
        if (socket)
            QObject::disconnect(socket, nullptr, this, nullptr);
}
bool DeviceStateServer::listen() {
    lease_clock_.start();
    lease_timer_.setInterval(500);
    connect(&lease_timer_, &QTimer::timeout, this, &DeviceStateServer::update_battery_watch);
    server_.setSocketOptions(QLocalServer::UserAccessOption);
    connect(&server_, &QLocalServer::newConnection, this, &DeviceStateServer::accept);
    current_ = encode_device_state({});
    return server_.listen(endpoint());
}
void DeviceStateServer::accept() {
    std::erase_if(clients_, [](const auto &client) { return client.isNull(); });
    while (auto *socket = server_.nextPendingConnection()) {
        if (clients_.size() >= 32) {
            socket->abort();
            socket->deleteLater();
            continue;
        }
        clients_.emplace_back(socket);
        connect(socket, &QLocalSocket::readyRead, this, [this, socket] { receive(socket); });
        connect(socket, &QLocalSocket::disconnected, socket, &QObject::deleteLater);
        connect(socket, &QLocalSocket::disconnected, this, &DeviceStateServer::update_battery_watch);
        socket->write(current_);
    }
}
void DeviceStateServer::receive(QLocalSocket *socket) {
    if (socket->bytesAvailable() > 64) {
        socket->abort();
        return;
    }
    while (socket->canReadLine()) {
        if (!request_refresh(socket, socket->readLine().trimmed())) {
            socket->abort();
            return;
        }
    }
}

bool DeviceStateServer::request_refresh(QLocalSocket *socket, const QByteArray &line) {
    if (line.startsWith("battery-watch "))
        return request_battery_watch(socket, line.mid(14));
    if (line == "battery") {
        emit batteryRefreshRequested();
        return true;
    }
    if (line != "refresh") {
        if (!line.startsWith("refresh "))
            return false;
        bool valid{};
        const auto token = line.mid(8).toULongLong(&valid);
        if (!valid || token == 0)
            return false;
        socket->setProperty("refreshToken", QByteArray::number(token));
    }
    emit refreshRequested();
    return true;
}
bool DeviceStateServer::request_battery_watch(QLocalSocket *socket, const QByteArray &line) {
    bool valid{};
    const int interval = line.toInt(&valid);
    if (!valid || (interval != 0 && !application::valid_battery_interval(interval)))
        return false;
    socket->setProperty("batteryInterval", interval);
    socket->setProperty("batteryLeaseUntil", lease_clock_.elapsed() + 3500);
    update_battery_watch();
    return true;
}
int DeviceStateServer::requested_battery_interval() const {
    int interval{};
    for (const auto &socket : clients_) {
        if (!socket || socket->state() != QLocalSocket::ConnectedState ||
            socket->property("batteryLeaseUntil").toLongLong() <= lease_clock_.elapsed())
            continue;
        const int requested = socket->property("batteryInterval").toInt();
        if (requested > 0 && (interval == 0 || requested < interval))
            interval = requested;
    }
    return interval;
}
void DeviceStateServer::update_battery_watch() {
    const int interval = requested_battery_interval();
    if (interval > 0 && !lease_timer_.isActive())
        lease_timer_.start();
    if (interval == 0)
        lease_timer_.stop();
    if (interval == battery_interval_)
        return;
    battery_interval_ = interval;
    emit batteryWatchChanged(interval);
}
void DeviceStateServer::finishRefresh(bool success) {
    for (const auto &socket : clients_) {
        if (!socket)
            continue;
        const auto token = socket->property("refreshToken").toByteArray();
        if (token.isEmpty())
            continue;
        socket->setProperty("refreshToken", QByteArray{});
        if (socket->bytesToWrite() > frame_limit)
            socket->abort();
        else
            socket->write(QByteArray(success ? "refreshed " : "refresh-failed ") + token + '\n');
    }
}
void DeviceStateServer::publish(const application::overview::DeviceState &state) {
    current_ = encode_device_state(state);
    for (const auto &socket : clients_) {
        if (!socket)
            continue;
        if (socket->bytesToWrite() > frame_limit)
            socket->abort();
        else
            socket->write(current_);
    }
}
DeviceStateClient::DeviceStateClient(QObject *parent) : QObject(parent) {
    refresh_deadline_.setSingleShot(true);
    refresh_deadline_.setInterval(15000);
    connect(&refresh_deadline_, &QTimer::timeout, this,
            [this] { finish_refresh(application::overview::RefreshResult::TimedOut); });
    connect(&socket_, &QLocalSocket::readyRead, this, &DeviceStateClient::receive);
    connect(&socket_, &QLocalSocket::connected, this, &DeviceStateClient::refresh);
    connect(&socket_, &QLocalSocket::disconnected, this, &DeviceStateClient::disconnected);
    connect(&socket_, &QLocalSocket::errorOccurred, this, [this] { disconnected(); });
}

DeviceStateClient::~DeviceStateClient() {
    // Socket teardown must not call back after the timer member has been destroyed.
    QObject::disconnect(&socket_, nullptr, this, nullptr);
}
void DeviceStateClient::start() {
    if (socket_.state() == QLocalSocket::UnconnectedState)
        socket_.connectToServer(endpoint());
}
void DeviceStateClient::refresh() {
    ++refresh_token_;
    refresh_deadline_.start();
    emit refreshStarted();
    if (socket_.state() == QLocalSocket::ConnectedState)
        socket_.write("refresh " + QByteArray::number(refresh_token_) + '\n');
    else
        start();
}
void DeviceStateClient::refreshBattery() {
    if (socket_.state() == QLocalSocket::ConnectedState)
        socket_.write("battery\n");
    else
        start();
}
void DeviceStateClient::watchBattery(int intervalMs) {
    if (socket_.state() == QLocalSocket::ConnectedState)
        socket_.write("battery-watch " + QByteArray::number(intervalMs) + '\n');
    else if (intervalMs != 0)
        start();
}
void DeviceStateClient::receive() {
    if (socket_.bytesAvailable() > frame_limit) {
        socket_.abort();
        return;
    }
    while (socket_.canReadLine()) {
        const auto line = socket_.readLine(frame_limit);
        if (receive_refresh(line.trimmed()))
            continue;
        const auto state = decode_device_state(line);
        if (state)
            emit stateChanged(*state);
        else {
            socket_.abort();
            return;
        }
    }
}

bool DeviceStateClient::receive_refresh(const QByteArray &line) {
    const bool completed = line.startsWith("refreshed ");
    if (!completed && !line.startsWith("refresh-failed "))
        return false;
    const auto token = line.mid(completed ? 10 : 15);
    if (token == QByteArray::number(refresh_token_))
        finish_refresh(completed ? application::overview::RefreshResult::Completed
                                 : application::overview::RefreshResult::Failed);
    return true;
}
void DeviceStateClient::finish_refresh(application::overview::RefreshResult result) {
    if (!refresh_deadline_.isActive() && result != application::overview::RefreshResult::TimedOut)
        return;
    refresh_deadline_.stop();
    emit refreshFinished(result);
}
void DeviceStateClient::disconnected() {
    finish_refresh(application::overview::RefreshResult::Unavailable);
    emit unavailable();
}
} // namespace manager::adapters
