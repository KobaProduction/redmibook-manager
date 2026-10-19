#include "adapters/local_api/session_server.hpp"
#include <QCoreApplication>
#include <QTimer>
#include <QUuid>
#include <atomic>
#include <chrono>
#include <thread>

namespace {
class SlowCore final : public manager::application::DiagnosticPort {
  public:
    mutable std::atomic<int> calls{0};
    manager::application::DiagnosticReply probe() const override {
        ++calls;
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        return {.connected = true, .version = "0.1.0", .build = "test", .process_id = 1};
    }
};
class Feedback final : public manager::application::DiagnosticFeedbackPort {
  public:
    int calls{0};
    void show_result(const manager::application::DiagnosticReply &) override {
        ++calls;
    }
};
class Client final : public QObject {
  public:
    QLocalSocket socket;
    QByteArray message;
    void connect_to(const QString &endpoint, const QByteArray &command) {
        connect(&socket, &QLocalSocket::connected, this, [this, command] { socket.write(command); });
        connect(&socket, &QLocalSocket::readyRead, this, [this] { message += socket.readAll(); });
        socket.connectToServer(endpoint);
    }
};
bool valid(const Client &client) {
    return client.message.startsWith("RMB/1 OK ") && client.message.contains(" up 1 0.1.0 test ") &&
           client.message.endsWith(QByteArray(MANAGER_APP_VERSION) + ' ' + MANAGER_BUILD_ID + '\n');
}
} // namespace
int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);
    SlowCore core;
    Feedback feedback;
    const manager::application::ReadRuntimeStatus read(core);
    manager::adapters::SessionServer server(read, feedback);
    const auto endpoint =
        QStringLiteral("RedmiBook.SessionTest.") + QUuid::createUuid().toString(QUuid::Id128);
    if (!server.listen(endpoint)) {
        return 1;
    }
    Client status;
    Client diagnostic;
    status.connect_to(endpoint, "RMB/1 status\n");
    diagnostic.connect_to(endpoint, "RMB/1 diagnostic\n");
    QTimer::singleShot(700, &app, [&] {
        QCoreApplication::exit(
            valid(status) && valid(diagnostic) && core.calls == 1 && feedback.calls == 1 ? 0 : 1);
    });
    return app.exec();
}
