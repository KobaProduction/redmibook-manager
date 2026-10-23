#include "presentation/common/runtime_monitor.hpp"
#include <QCoreApplication>
#include <QTimer>
#include <array>

namespace {
using manager::application::ConnectionHealth;
manager::application::SessionReply healthy() {
    manager::application::SessionReply reply;
    reply.connected = true;
    reply.daemon = {
        .connected = true, .version = MANAGER_APP_VERSION, .build = MANAGER_BUILD_ID, .process_id = 1};
    reply.session_version = MANAGER_APP_VERSION;
    reply.session_build = MANAGER_BUILD_ID;
    return reply;
}
ConnectionHealth health(const manager::application::SessionReply &reply) {
    return manager::application::connection_health(reply, {MANAGER_APP_VERSION, MANAGER_BUILD_ID});
}
bool identity_contract() {
    auto reply = healthy();
    if (health(reply) != ConnectionHealth::Ready) {
        return false;
    }
    reply.daemon.version = "other-version";
    if (health(reply) != ConnectionHealth::VersionMismatch) {
        return false;
    }
    reply = healthy();
    reply.daemon.build = "other-build";
    if (health(reply) != ConnectionHealth::VersionMismatch) {
        return false;
    }
    reply = healthy();
    reply.session_build = "old-session";
    if (health(reply) != ConnectionHealth::VersionMismatch) {
        return false;
    }
    reply.session_build.clear();
    return health(reply) == ConnectionHealth::VersionMismatch;
}
class SessionFake final : public manager::application::SessionPort {
  public:
    manager::application::SessionReply value{healthy()};
    bool request(manager::application::SessionCommand,
                 std::function<void(manager::application::SessionReply)> completion) override {
        completion(value);
        return true;
    }
};
class HeartbeatContract final {
  public:
    SessionFake source;
    manager::presentation::RuntimeMonitor monitor{source};
    void changed() {
        if (monitor.pending()) {
            return;
        }
        if (health(monitor.reply()) != expected_.at(phase_)) {
            QCoreApplication::exit(1);
            return;
        }
        switch (phase_++) {
        case 0:
            source.value.daemon.connected = false;
            break;
        case 1:
            source.value.connected = false;
            break;
        case 2:
            source.value = healthy();
            break;
        default:
            QCoreApplication::exit(0);
            break;
        }
    }

  private:
    std::size_t phase_{0};
    const std::array<ConnectionHealth, 4> expected_{
        ConnectionHealth::Ready, ConnectionHealth::CoreUnavailable, ConnectionHealth::SessionUnavailable,
        ConnectionHealth::Ready};
};
} // namespace
int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);
    if (!identity_contract()) {
        return 1;
    }
    {
        SessionFake source;
        manager::presentation::RuntimeMonitor monitor(source);
        monitor.set_foreground(false);
        source.value.connected = false;
        monitor.set_foreground(true);
        if (health(monitor.reply()) != ConnectionHealth::SessionUnavailable)
            return 1;
    }
    HeartbeatContract contract;
    QObject::connect(&contract.monitor, &manager::presentation::RuntimeMonitor::changed, &app,
                     [&contract] { contract.changed(); });
    QTimer::singleShot(9000, &app, [] { QCoreApplication::exit(1); });
    return app.exec();
}
