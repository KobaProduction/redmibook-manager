#include "adapters/local_api/nfc_client.hpp"
#include "adapters/local_api/nfc_codec.hpp"
#include <QJsonDocument>

namespace manager::adapters {
using application::nfc::Status;
NfcClient::NfcClient(QString endpoint, QObject *parent) : QObject(parent), endpoint_(std::move(endpoint)) {
    deadline_.setSingleShot(true);
    deadline_.setInterval(10000);
    connect(&deadline_, &QTimer::timeout, this, [this] {
        unavailable(Status::UnknownOutcome);
        refresh();
    });
    connect(&socket_, &QLocalSocket::readyRead, this, &NfcClient::receive);
    connect(&socket_, &QLocalSocket::disconnected, this, [this] { unavailable(Status::Unavailable); });
    connect(&socket_, &QLocalSocket::errorOccurred, this, [this] { unavailable(Status::Unavailable); });
}
NfcClient::~NfcClient() {
    QObject::disconnect(&socket_, nullptr, this, nullptr);
}
void NfcClient::observe(std::function<void(const application::nfc::Snapshot &)> observer) {
    observer_ = std::move(observer);
    publish();
    if (observer_)
        start();
}
void NfcClient::start() {
    if (socket_.state() == QLocalSocket::UnconnectedState)
        socket_.connectToServer(endpoint_);
}
void NfcClient::refresh() {
    if (socket_.state() != QLocalSocket::ConnectedState) {
        start();
        return;
    }
    socket_.write(nfc_wire::frame(nfc_wire::envelope("read")));
}
bool NfcClient::write(application::nfc::WriteRequest request, std::function<void(Status)> completion) {
    if (completion_ || socket_.state() != QLocalSocket::ConnectedState || state_.status != Status::Ready)
        return false;
    auto object = nfc_wire::envelope("write");
    object.insert("token", QString::number(++token_));
    object.insert("expected", nfc_wire::bytes(request.expected));
    object.insert("content", nfc_wire::bytes(request.content));
    completion_ = std::move(completion);
    deadline_.start();
    socket_.write(nfc_wire::frame(object));
    return true;
}
void NfcClient::publish() {
    if (observer_)
        observer_(state_);
}
void NfcClient::finish(Status status) {
    deadline_.stop();
    auto callback = std::move(completion_);
    completion_ = {};
    if (callback)
        callback(status);
}
void NfcClient::unavailable(Status status) {
    if (state_.status == Status::Incompatible)
        status = Status::Incompatible;
    state_.status = status;
    state_.writable = false;
    state_.field.reset();
    publish();
    finish(status);
}
void NfcClient::accept(const QJsonObject &object) {
    if (!nfc_wire::compatible(object)) {
        unavailable(Status::Incompatible);
        socket_.abort();
        return;
    }
    const auto state = nfc_wire::parse_snapshot(object.value("state").toObject());
    if (!state) {
        socket_.abort();
        return;
    }
    state_ = *state;
    publish();
    if (object.value("kind").toString() == "result" &&
        object.value("token").toString() == QString::number(token_)) {
        const int status = object.value("status").toInt(-1);
        finish(status >= 0 && status <= static_cast<int>(Status::Incompatible) ? static_cast<Status>(status)
                                                                               : Status::Failed);
    }
}
void NfcClient::receive() {
    if (socket_.bytesAvailable() > 16384) {
        socket_.abort();
        return;
    }
    while (socket_.canReadLine()) {
        const auto document = QJsonDocument::fromJson(socket_.readLine(16384));
        if (!document.isObject()) {
            socket_.abort();
            return;
        }
        accept(document.object());
    }
}
} // namespace manager::adapters
