#include "adapters/local_api/preference_authority.hpp"
#include "presentation/common/preferences.hpp"
#include <QApplication>
#include <QEventLoop>
#include <QJsonDocument>
#include <QProcess>
#include <QTemporaryDir>
#include <QTextStream>
#include <QUuid>

namespace {
using namespace manager::adapters;
bool await(PreferenceClient &client, const std::function<bool()> &ready) {
    if (ready())
        return true;
    QEventLoop loop;
    QTimer deadline;
    deadline.setSingleShot(true);
    QObject::connect(&deadline, &QTimer::timeout, &loop, &QEventLoop::quit);
    client.observe([&] {
        if (ready())
            loop.quit();
    });
    deadline.start(3000);
    loop.exec();
    client.observe({});
    return ready();
}
bool await_connection(PreferenceClient &client, manager::application::PreferenceConnection state) {
    if (client.connection() == state)
        return true;
    QEventLoop loop;
    QTimer deadline;
    deadline.setSingleShot(true);
    QObject::connect(&deadline, &QTimer::timeout, &loop, &QEventLoop::quit);
    client.observe_connection([&] {
        if (client.connection() == state)
            loop.quit();
    });
    deadline.start(3000);
    loop.exec();
    client.observe_connection({});
    return client.connection() == state;
}
struct TestAddress {
    QString directory;
    QString endpoint;
};
bool language_is(const PreferenceClient &client, const char *language) {
    const auto state = client.read();
    return state && state->language == language;
}
bool theme_is(const PreferenceClient &client, const char *theme) {
    const auto state = client.read();
    return state && state->theme == theme;
}
bool incompatible_server(PreferenceClient &client, const QString &endpoint) {
    QLocalServer server;
    if (!server.listen(endpoint))
        return false;
    QObject::connect(&server, &QLocalServer::newConnection, &server, [&] {
        auto *peer = server.nextPendingConnection();
        QObject::connect(peer, &QLocalSocket::readyRead, peer, [peer] {
            peer->readAll();
            peer->write("{\"protocol\":-1}\n");
        });
    });
    client.reconnect();
    return await_connection(client, manager::application::PreferenceConnection::Incompatible) &&
           !client.reset_defaults();
}
class RecoveryCase {
  public:
    explicit RecoveryCase(const TestAddress &address)
        : endpoint_(address.endpoint), client_(endpoint_), store_(address.directory, true) {
        store_.observe([this] {
            if (server_)
                server_->broadcast();
        });
    }
    bool run() {
        using manager::application::PreferenceConnection;
        if (!await_connection(client_, PreferenceConnection::Unavailable) ||
            client_.update("theme", "dark") || !start())
            return false;
        const auto before = client_.read();
        if (!before || !stop())
            return false;
        const auto cached = client_.read();
        if (client_.update("theme", "dark") || !cached || cached->revision != before->revision || !start())
            return false;
        if (!client_.update("theme", "dark") ||
            !await(client_, [this] { return theme_is(client_, "dark"); }) || !stop())
            return false;
        return incompatible_server(client_, endpoint_);
    }

  private:
    QString endpoint_;
    PreferenceClient client_;
    UiPreferenceStore store_;
    std::unique_ptr<PreferenceServer> server_;
    bool start() {
        server_ = std::make_unique<PreferenceServer>(store_, endpoint_);
        return server_->listen() &&
               await_connection(client_, manager::application::PreferenceConnection::Ready);
    }
    bool stop() {
        server_.reset();
        return await_connection(client_, manager::application::PreferenceConnection::Unavailable);
    }
};
bool recovery_contract(const TestAddress &address) {
    return RecoveryCase(address).run();
}
bool exchange_preferences(PreferenceAuthority &first, PreferenceAuthority &second) {
    if (!first.client().update("theme", "dark") ||
        !await(second.client(), [&] { return theme_is(second.client(), "dark"); }))
        return false;
    return second.client().update("language", "ru") &&
           await(first.client(), [&] { return language_is(first.client(), "ru"); });
}
bool authority_handover(const TestAddress &address) {
    using manager::application::PreferenceConnection;
    auto first = std::make_unique<PreferenceAuthority>(address.directory, address.endpoint);
    PreferenceAuthority second(address.directory, address.endpoint);
    if (!first->owns_authority() || second.owns_authority() ||
        !await_connection(first->client(), PreferenceConnection::Ready) ||
        !await_connection(second.client(), PreferenceConnection::Ready) ||
        !exchange_preferences(*first, second))
        return false;
    first.reset();
    if (!await(second.client(), [&] {
            return second.owns_authority() && second.client().connection() == PreferenceConnection::Ready;
        }))
        return false;
    const auto state = second.client().read();
    return second.owns_authority() && state && state->theme == "dark" && state->language == "ru";
}
class HandshakeCase {
  public:
    explicit HandshakeCase(const TestAddress &address)
        : endpoint_(address.endpoint), store_(address.directory, true), server_(store_, endpoint_) {
        deadline_.setSingleShot(true);
        QObject::connect(&deadline_, &QTimer::timeout, &loop_, &QEventLoop::quit);
        QObject::connect(&socket_, &QLocalSocket::connected, &socket_, [this] { send_hello(); });
        QObject::connect(&socket_, &QLocalSocket::readyRead, &socket_, [this] { receive(); });
    }
    bool run() {
        if (!server_.listen())
            return false;
        socket_.connectToServer(endpoint_);
        deadline_.start(3000);
        loop_.exec();
        const auto state = store_.read();
        return rejected_ && state && state->revision == 0;
    }

  private:
    QString endpoint_;
    UiPreferenceStore store_;
    PreferenceServer server_;
    QLocalSocket socket_;
    QEventLoop loop_;
    QTimer deadline_;
    bool rejected_{false};
    void send_hello() {
        const QJsonObject hello{{"protocol", 1},
                                {"version", MANAGER_APP_VERSION},
                                {"build", "other-build"},
                                {"op", "hello"},
                                {"id", 1}};
        socket_.write(QJsonDocument(hello).toJson(QJsonDocument::Compact) + '\n');
    }
    void receive() {
        if (!socket_.canReadLine())
            return;
        const auto reply = QJsonDocument::fromJson(socket_.readLine()).object();
        rejected_ = reply.value("error") == "incompatible" && reply.value("build") == MANAGER_BUILD_ID &&
                    !reply.value("ok").toBool();
        loop_.quit();
    }
};
bool production_handshake_rejection(const TestAddress &address) {
    return HandshakeCase(address).run();
}
class ObserverProcess final : public QProcess {
  public:
    ~ObserverProcess() override {
        kill();
        waitForFinished(1000);
    }
};
struct ExpectedLanguage {
    QString language;
    QString caption;
};
bool consume_language(QProcess &child, const ExpectedLanguage &expected) {
    bool matched = false;
    while (child.canReadLine()) {
        const auto value = QJsonDocument::fromJson(child.readLine()).object();
        matched = matched || (value.value("language") == expected.language &&
                              value.value("caption") == expected.caption);
    }
    return matched;
}
bool await_language(QProcess &child, const ExpectedLanguage &expected) {
    bool matched = false;
    QEventLoop loop;
    QTimer deadline;
    deadline.setSingleShot(true);
    const auto consume = [&] {
        matched = consume_language(child, expected) || matched;
        if (matched)
            loop.quit();
    };
    const auto connection = QObject::connect(&child, &QProcess::readyReadStandardOutput, &loop, consume);
    QObject::connect(&deadline, &QTimer::timeout, &loop, &QEventLoop::quit);
    consume();
    if (!matched) {
        deadline.start(3000);
        loop.exec();
    }
    QObject::disconnect(connection);
    return matched;
}
int observe(QApplication &app, const QString &endpoint) {
    PreferenceClient client(endpoint);
    manager::presentation::Preferences preferences(client);
    const auto publish = [&] {
        QTextStream output(stdout);
        output << QJsonDocument(QJsonObject{{"language", preferences.language()},
                                            {"caption", QCoreApplication::translate("Main", "Overview")}})
                      .toJson(QJsonDocument::Compact)
               << '\n';
        output.flush();
    };
    QObject::connect(&preferences, &manager::presentation::Preferences::changed, &app, publish);
    QTimer::singleShot(12000, &app, &QCoreApplication::quit);
    return app.exec();
}

bool reject_unknown(PreferenceClient &client, const UiPreferenceStore &store) {
    bool rejected = false;
    QEventLoop loop;
    QTimer deadline;
    deadline.setSingleShot(true);
    QObject::connect(&deadline, &QTimer::timeout, &loop, &QEventLoop::quit);
    client.observe_error([&] {
        rejected = true;
        loop.quit();
    });
    if (!client.update("unknown-key", "invalid")) {
        client.observe_error({});
        return false;
    }
    deadline.start(3000);
    loop.exec();
    client.observe_error({});
    const auto state = store.read();
    return rejected && state && state->theme == "dark";
}
bool translated_fanout(PreferenceClient &first, PreferenceClient &second, ObserverProcess &observer) {
    if (!first.update("language", "ru") || !await(second, [&] { return language_is(second, "ru"); }) ||
        !await(first, [&] { return language_is(first, "ru"); }))
        return false;
    if (!second.update("theme", "dark") || !await(first, [&] { return theme_is(first, "dark"); }) ||
        !await_language(observer, {"ru", QString::fromUtf8("Обзор")}))
        return false;
    return first.update("language", "en") && await(second, [&] { return language_is(second, "en"); }) &&
           await_language(observer, {"en", "Overview"});
}
class AuthorityCase {
  public:
    explicit AuthorityCase(const TestAddress &address)
        : endpoint_(address.endpoint), store_(address.directory, true), server_(store_, endpoint_),
          first_(endpoint_), second_(endpoint_) {
        store_.observe([this] { server_.broadcast(); });
        store_.update("language", "en");
    }
    bool run() {
        if (!server_.listen() || !await(first_, [this] { return first_.read().has_value(); }) ||
            !await(second_, [this] { return second_.read().has_value(); }))
            return false;
        observer_.start(QCoreApplication::applicationFilePath(), {"--observe", endpoint_});
        return await_language(observer_, {"en", "Overview"}) &&
               translated_fanout(first_, second_, observer_) && reject_unknown(first_, store_);
    }

  private:
    QString endpoint_;
    UiPreferenceStore store_;
    PreferenceServer server_;
    PreferenceClient first_;
    PreferenceClient second_;
    ObserverProcess observer_;
};
bool additional_contracts(const QString &endpoint) {
    QTemporaryDir recovery, handover, mismatch;
    return recovery_contract({recovery.path(), endpoint + ".Recovery"}) &&
           authority_handover({handover.path(), endpoint + ".Authority"}) &&
           production_handshake_rejection({mismatch.path(), endpoint + ".Mismatch"});
}
} // namespace
int main(int argc, char **argv) {
    QApplication app(argc, argv);
    app.setStyle(QStringLiteral("Fusion"));
    if (app.arguments().size() == 3 && app.arguments().at(1) == "--observe")
        return observe(app, app.arguments().at(2));
    QTemporaryDir directory;
    const auto endpoint = "Redmibook.Preferences.Test." + QUuid::createUuid().toString(QUuid::WithoutBraces);
    if (!AuthorityCase({directory.path(), endpoint}).run() || !additional_contracts(endpoint))
        return 1;
    UiPreferenceStore disk(directory.path());
    const auto saved = disk.read();
    return saved && saved->language == "en" && saved->theme == "dark" ? 0 : 2;
}
