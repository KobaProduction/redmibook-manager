#include "adapters/local_api/device_state_codec.hpp"
#include "adapters/local_api/device_state_stream.hpp"
#include "adapters/local_api/session_client.hpp"
#include <QCoreApplication>
#include <QLocalServer>
#include <QLocalSocket>
#include <QTimer>
#include <array>

namespace {
using manager::application::overview::RefreshResult;
class RefreshScenario final : public QObject {
  public:
    explicit RefreshScenario(QCoreApplication &app) : app_(app) {
        connect(&server_, &QLocalServer::newConnection, this, &RefreshScenario::accept);
        connect(&client_, &manager::adapters::DeviceStateClient::stateChanged, this, &RefreshScenario::state);
        connect(&client_, &manager::adapters::DeviceStateClient::refreshStarted, this,
                [this] { ++started_; });
        connect(&client_, &manager::adapters::DeviceStateClient::refreshFinished, this,
                &RefreshScenario::finished);
        QTimer::singleShot(3000, this, [this] { app_.exit(1); });
    }
    bool start() {
        if (!server_.listen(manager::adapters::session_endpoint() + QStringLiteral(".DeviceState.v1")))
            return false;
        client_.start();
        return true;
    }

  private:
    void accept() {
        socket_ = server_.nextPendingConnection();
        connect(socket_, &QLocalSocket::readyRead, this, &RefreshScenario::receive);
        socket_->write(manager::adapters::encode_device_state({}));
    }
    void receive() {
        while (socket_->canReadLine()) {
            const auto line = socket_->readLine().trimmed();
            if (!line.startsWith("refresh ")) {
                app_.exit(1);
                return;
            }
            token_ = line.mid(8);
            reply();
        }
    }
    void reply() {
        ++requests_;
        if (requests_ == 1) {
            socket_->write("refreshed 999999999\n");
            manager::application::overview::DeviceState snapshot;
            snapshot.revision = 7;
            socket_->write(manager::adapters::encode_device_state(snapshot));
        } else if (requests_ == 2) {
            socket_->write("refresh-failed " + token_ + '\n');
        } else {
            socket_->abort();
        }
    }
    void state(const manager::application::overview::DeviceState &value) {
        if (value.revision != 7)
            return;
        fresh_ = true;
        socket_->write("refreshed " + token_ + '\n');
    }
    void finished(RefreshResult result) {
        constexpr std::array expected{RefreshResult::Completed, RefreshResult::Failed,
                                      RefreshResult::Unavailable};
        if (!fresh_ || finished_ >= expected.size() || expected[finished_] != result) {
            app_.exit(1);
            return;
        }
        ++finished_;
        if (finished_ == expected.size())
            app_.exit(started_ == 3 && requests_ == 3 ? 0 : 1);
        else
            client_.refresh();
    }
    QCoreApplication &app_;
    QLocalServer server_;
    QLocalSocket *socket_{};
    manager::adapters::DeviceStateClient client_;
    QByteArray token_;
    std::size_t finished_{};
    int started_{};
    int requests_{};
    bool fresh_{false};
};
} // namespace

namespace {
int battery_only(QCoreApplication &app) {
    manager::adapters::DeviceStateServer server;
    manager::adapters::DeviceStateClient client;
    int full_refreshes{};
    QObject::connect(&server, &manager::adapters::DeviceStateServer::refreshRequested, &app, [&] {
        ++full_refreshes;
        server.finishRefresh(true);
    });
    QObject::connect(&client, &manager::adapters::DeviceStateClient::refreshFinished, &app,
                     [&](RefreshResult result) {
                         if (result == RefreshResult::Completed)
                             client.refreshBattery();
                         else
                             app.exit(1);
                     });
    QObject::connect(&server, &manager::adapters::DeviceStateServer::batteryRefreshRequested, &app,
                     [&] { app.exit(full_refreshes == 1 ? 0 : 1); });
    QTimer::singleShot(3000, &server, [&] { app.exit(1); });
    if (!server.listen())
        return 1;
    client.start();
    return app.exec();
}
} // namespace
namespace {
class BatteryLeaseScenario {
  public:
    explicit BatteryLeaseScenario(QCoreApplication &app) : app_(app) {
        QObject::connect(&server_, &manager::adapters::DeviceStateServer::refreshRequested, &server_, [this] {
            ++full_;
            server_.finishRefresh(true);
        });
        QObject::connect(&client_, &manager::adapters::DeviceStateClient::refreshFinished, &server_,
                         [this](RefreshResult result) { refreshed(result); });
        QObject::connect(&server_, &manager::adapters::DeviceStateServer::batteryWatchChanged, &server_,
                         [this](int interval) { watch_changed(interval); });
        QTimer::singleShot(6000, &server_, [this] { app_.exit(1); });
    }
    int run() {
        if (!server_.listen())
            return 1;
        client_.start();
        return app_.exec();
    }

  private:
    QCoreApplication &app_;
    manager::adapters::DeviceStateServer server_;
    manager::adapters::DeviceStateClient client_;
    static constexpr std::array expected{5000, 100, 0, 1000, 0};
    std::size_t event_{};
    int full_{};
    void refreshed(RefreshResult result) {
        if (result == RefreshResult::Completed)
            client_.watchBattery(5000);
        else
            app_.exit(1);
    }
    void watch_changed(int interval) {
        if (event_ >= expected.size() || interval != expected[event_]) {
            app_.exit(1);
            return;
        }
        ++event_;
        if (event_ == expected.size()) {
            app_.exit(full_ == 1 ? 0 : 1);
            return;
        }
        if (event_ < expected.size() - 1)
            client_.watchBattery(expected[event_]);
        // Deliberately stop renewal: the last lease must expire.
    }
};
int battery_lease(QCoreApplication &app) {
    return BatteryLeaseScenario(app).run();
}

} // namespace
int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);
    qputenv("MANAGER_DEV_SCOPE",
            "refresh-contract-" + QByteArray::number(QCoreApplication::applicationPid()));
    {
        RefreshScenario scenario(app);
        const auto result = scenario.start() ? app.exec() : 1;
        if (result != 0)
            return result;
    }
    qputenv("MANAGER_DEV_SCOPE",
            "battery-contract-" + QByteArray::number(QCoreApplication::applicationPid()));
    const auto result = battery_only(app);
    if (result != 0)
        return result;
    qputenv("MANAGER_DEV_SCOPE", "lease-contract-" + QByteArray::number(QCoreApplication::applicationPid()));
    return battery_lease(app);
}
