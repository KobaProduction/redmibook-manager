#include "adapters/local_api/nfc_server.hpp"
#include "adapters/local_api/nfc_codec.hpp"
#include "application/nfc/draft.hpp"
#include <QJsonDocument>
#include <QtConcurrentRun>
#include <algorithm>

namespace manager::adapters {
namespace {
using application::nfc::Bytes;
using application::nfc::Result;
using application::nfc::Status;
void send(QLocalSocket *socket, const QJsonObject &object) {
    if (socket->bytesToWrite() > 16384)
        socket->abort();
    else
        socket->write(nfc_wire::frame(object));
}
} // namespace
NfcServer::NfcServer(std::shared_ptr<application::nfc::TagPort> tag, QThreadPool &pool, QString endpoint,
                     QObject *parent)
    : QObject(parent), tag_(std::move(tag)), pool_(pool), endpoint_(std::move(endpoint)) {
    delivery_->target = this;
    deadline_.setSingleShot(true);
    deadline_.setInterval(10000);
    connect(&server_, &QLocalServer::newConnection, this, &NfcServer::accept);
    connect(&pending_, &QFutureWatcher<Result>::finished, this, &NfcServer::complete);
    connect(&deadline_, &QTimer::timeout, this, [this] {
        expired_ = true;
        auto state = state_;
        state.status = writer_ ? Status::UnknownOutcome : Status::TimedOut;
        state.writable = false;
        publish(state);
        if (writer_)
            result(writer_, token_, Status::UnknownOutcome);
        writer_.clear();
    });
}
NfcServer::~NfcServer() {
    {
        const std::lock_guard lock(delivery_->mutex);
        delivery_->target = nullptr;
    }
    watch_.reset();
    QObject::disconnect(&server_, nullptr, this, nullptr);
    for (const auto &socket : clients_)
        if (socket)
            QObject::disconnect(socket, nullptr, this, nullptr);
}
bool NfcServer::listen() {
    server_.setSocketOptions(QLocalServer::UserAccessOption);
    const bool listening = server_.listen(endpoint_);
    if (listening)
        refresh();
    return listening;
}
void NfcServer::accept() {
    std::erase_if(clients_, [](const auto &client) { return client.isNull(); });
    while (auto *socket = server_.nextPendingConnection()) {
        if (clients_.size() >= 16) {
            socket->abort();
            socket->deleteLater();
            continue;
        }
        clients_.emplace_back(socket);
        connect(socket, &QLocalSocket::readyRead, this, [this, socket] { receive(socket); });
        connect(socket, &QLocalSocket::disconnected, socket, &QObject::deleteLater);
        auto frame = nfc_wire::envelope("state");
        frame.insert("state", nfc_wire::snapshot(state_));
        send(socket, frame);
    }
}
void NfcServer::begin() {
    busy_ = true;
    expired_ = false;
    deadline_.start();
}
void NfcServer::refresh() {
    if (busy_) {
        again_ = true;
        return;
    }
    begin();
    pending_.setFuture(QtConcurrent::run(&pool_, [tag = tag_] {
        auto state = tag->read();
        return Result{state.status, std::move(state)};
    }));
}
void NfcServer::receive(QLocalSocket *socket) {
    if (socket->bytesAvailable() > 16384) {
        socket->abort();
        return;
    }
    while (socket->canReadLine()) {
        const auto document = QJsonDocument::fromJson(socket->readLine(16384));
        if (!document.isObject()) {
            socket->abort();
            return;
        }
        request(socket, document.object());
    }
}
void NfcServer::request(QLocalSocket *socket, const QJsonObject &object) {
    if (!nfc_wire::compatible(object)) {
        socket->abort();
        return;
    }
    const auto kind = object.value("kind").toString();
    if (kind == "read")
        refresh();
    else if (kind == "write")
        write(socket, object);
    else
        socket->abort();
}
void NfcServer::write(QLocalSocket *socket, const QJsonObject &object) {
    const auto token = object.value("token").toString();
    const auto expected = nfc_wire::parse_bytes(object.value("expected"));
    const auto content = nfc_wire::parse_bytes(object.value("content"));
    if (token.isEmpty() || token.size() > 20 || !expected || !content) {
        socket->abort();
        return;
    }
    if (busy_) {
        result(socket, token, Status::Busy);
        return;
    }
    writer_ = socket;
    token_ = token;
    begin();
    pending_.setFuture(QtConcurrent::run(
        &pool_, [tag = tag_, request = application::nfc::WriteRequest{*expected, *content}] {
            return application::nfc::write_tag(*tag, request);
        }));
}
void NfcServer::watch() {
    if (watch_ || !state_.present)
        return;
    const auto delivery = delivery_;
    watch_ = tag_->subscribe([delivery] {
        const std::lock_guard lock(delivery->mutex);
        auto *target = delivery->target;
        if (!target || std::exchange(delivery->queued, true))
            return;
        QMetaObject::invokeMethod(
            target,
            [target, delivery] {
                {
                    const std::lock_guard delivery_lock(delivery->mutex);
                    delivery->queued = false;
                }
                target->refresh();
            },
            Qt::QueuedConnection);
    });
}
void NfcServer::publish(const application::nfc::Snapshot &state) {
    if (state == state_)
        return;
    state_ = state;
    auto frame = nfc_wire::envelope("state");
    frame.insert("state", nfc_wire::snapshot(state_));
    for (const auto &socket : clients_)
        if (socket)
            send(socket, frame);
}
void NfcServer::result(QLocalSocket *socket, const QString &token, Status status) {
    auto frame = nfc_wire::envelope("result");
    frame.insert("token", token);
    frame.insert("status", static_cast<int>(status));
    frame.insert("state", nfc_wire::snapshot(state_));
    send(socket, frame);
}
void NfcServer::complete() {
    deadline_.stop();
    const auto reply = pending_.result();
    busy_ = false;
    if (!expired_)
        publish(reply.snapshot);
    if (writer_)
        result(writer_, token_, reply.status);
    writer_.clear();
    watch();
    if (std::exchange(again_, false))
        refresh();
}
} // namespace manager::adapters
