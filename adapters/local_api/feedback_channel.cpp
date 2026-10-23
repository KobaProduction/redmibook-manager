#include "adapters/local_api/feedback_channel.hpp"
#include "adapters/local_api/session_client.hpp"
#include <QLocalSocket>
#include <QTimer>
namespace manager::adapters {
QByteArray encode_feedback(application::feedback::Event event) {
    return QByteArray("RMB/1 feedback ") + MANAGER_APP_VERSION + ' ' + MANAGER_BUILD_ID + ' ' +
           QByteArray::number(static_cast<unsigned int>(event.kind)) + ' ' + QByteArray::number(event.value) +
           '\n';
}
std::optional<application::feedback::Event> decode_feedback(const QByteArray &frame) {
    const auto fields = frame.trimmed().split(' ');
    if (fields.size() != 6 || fields[0] != "RMB/1" || fields[1] != "feedback" ||
        fields[2] != MANAGER_APP_VERSION || fields[3] != MANAGER_BUILD_ID)
        return {};
    bool kind_ok = false;
    bool value_ok = false;
    const auto kind = fields[4].toUInt(&kind_ok);
    const auto value = fields[5].toUInt(&value_ok);
    if (!kind_ok || !value_ok || kind > 2)
        return {};
    const application::feedback::Event event{static_cast<application::feedback::Kind>(kind), value};
    return application::feedback::valid(event) ? std::optional{event} : std::nullopt;
}
namespace {
void configure_feedback_socket(QLocalSocket *socket) {
    socket->setReadBufferSize(64);
    QObject::connect(socket, &QLocalSocket::readyRead, socket, [socket] {
        socket->readAll();
        socket->disconnectFromServer();
    });
    QObject::connect(socket, &QLocalSocket::disconnected, socket, &QObject::deleteLater);
    QObject::connect(socket, &QLocalSocket::errorOccurred, socket, &QObject::deleteLater);
    QTimer::singleShot(3000, socket, [socket] {
        socket->abort();
        socket->deleteLater();
    });
}
} // namespace
application::feedback::Delivery FeedbackClient::present(application::feedback::Event event) {
    using application::feedback::Delivery;
    if (!application::feedback::valid(event))
        return Delivery::Invalid;
    if (findChildren<QLocalSocket *>(QString{}, Qt::FindDirectChildrenOnly).size() >= 4)
        return Delivery::Unavailable;
    auto *socket = new QLocalSocket(this);
    configure_feedback_socket(socket);
    connect(socket, &QLocalSocket::connected, socket,
            [socket, event] { socket->write(encode_feedback(event)); });
    socket->connectToServer(session_endpoint());
    return Delivery::Queued;
}
} // namespace manager::adapters
