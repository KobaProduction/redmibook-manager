#include "adapters/local_api/device_state_stream.hpp"
#include "adapters/local_api/device_state_codec.hpp"
#include "adapters/local_api/session_client.hpp"
#include <algorithm>

namespace manager::adapters {
namespace {
QString endpoint() {
    return session_endpoint() + QStringLiteral(".DeviceState.v1");
}
constexpr qint64 frame_limit = 65536;
} // namespace
bool DeviceStateServer::listen() {
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
        socket->write(current_);
    }
    emit refreshRequested();
}
void DeviceStateServer::receive(QLocalSocket *socket) {
    if (socket->bytesAvailable() > 64) {
        socket->abort();
        return;
    }
    while (socket->canReadLine()) {
        if (socket->readLine().trimmed() == "refresh")
            emit refreshRequested();
        else
            socket->abort();
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
    connect(&socket_, &QLocalSocket::readyRead, this, &DeviceStateClient::receive);
    connect(&socket_, &QLocalSocket::connected, this, &DeviceStateClient::refresh);
    connect(&socket_, &QLocalSocket::disconnected, this, &DeviceStateClient::unavailable);
    connect(&socket_, &QLocalSocket::errorOccurred, this, [this] { emit unavailable(); });
}
void DeviceStateClient::start() {
    if (socket_.state() == QLocalSocket::UnconnectedState)
        socket_.connectToServer(endpoint());
}
void DeviceStateClient::refresh() {
    if (socket_.state() == QLocalSocket::ConnectedState)
        socket_.write("refresh\n");
    else
        start();
}
void DeviceStateClient::receive() {
    if (socket_.bytesAvailable() > frame_limit) {
        socket_.abort();
        return;
    }
    while (socket_.canReadLine()) {
        const auto state = decode_device_state(socket_.readLine(frame_limit));
        if (state)
            emit stateChanged(*state);
        else {
            socket_.abort();
            return;
        }
    }
}
} // namespace manager::adapters
