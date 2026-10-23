#include "tests/fixtures/preset_preview.hpp"
#include "apps/desktop/preview_presets.hpp"
#include "presentation/tray/dashboard.hpp"
#include <QApplication>
#include <QPushButton>
#include <QTimer>

namespace {
class Session final : public manager::application::SessionPort {
  public:
    bool request(manager::application::SessionCommand,
                 std::function<void(manager::application::SessionReply)> done) override {
        done({true,
              {true, MANAGER_APP_VERSION, MANAGER_BUILD_ID, 1},
              2,
              MANAGER_APP_VERSION,
              MANAGER_BUILD_ID});
        return true;
    }
};
class Executor final : public manager::application::presets::ExecutorPort {
  public:
    manager::presentation::TrayDeviceState state;
    manager::application::presets::Catalog choices() override {
        manager::application::presets::Catalog result;
        for (const auto &preset : state.presets)
            for (const auto &button : preset.buttons)
                for (const auto &action : button.actions)
                    result.push_back({action, {}});
        return result;
    }
    bool apply(const manager::application::presets::Action &action) override {
        using manager::application::presets::Kind;
        if (action.kind == Kind::Performance)
            state.profileIndex = std::stoi(action.value) - 1;
        if (action.kind == Kind::Refresh)
            state.refreshHz = std::stoi(action.value);
        return true;
    }
    void press(const QString &preset, const QString &button) {
        for (const auto &item : state.presets)
            if (item.id == preset.toStdString())
                for (const auto &entry : item.buttons)
                    if (entry.id == button.toStdString())
                        (void)manager::application::presets::execute(entry, *this);
    }
};
void initialize(Executor &executor) {
    executor.state.profileIndex = 0;
    executor.state.profileAvailable = true;
    executor.state.refreshAvailable = true;
    executor.state.microphoneAvailable = true;
    executor.state.refreshHz = 120;
    executor.state.refreshRates = {60, 120, 165};
    executor.state.chargeLimit = 80;
    executor.state.batteryRateW = -29.8;
    executor.state.presets = manager::preview::demo_presets();
    executor.state.presets.front().visible = true;
}
void bind(manager::presentation::TrayDashboard &dashboard, Executor &executor) {
    QObject::connect(&dashboard, &manager::presentation::TrayDashboard::profileRequested, &dashboard,
                     [&dashboard, &executor](int index) {
                         executor.state.profileIndex = index;
                         dashboard.setDeviceState(executor.state);
                     });
    QObject::connect(&dashboard, &manager::presentation::TrayDashboard::refreshRequested, &dashboard,
                     [&dashboard, &executor](unsigned int hz) {
                         executor.state.refreshHz = static_cast<int>(hz);
                         dashboard.setDeviceState(executor.state);
                     });
    QObject::connect(&dashboard, &manager::presentation::TrayDashboard::presetRequested, &dashboard,
                     [&dashboard, &executor](const QString &preset, const QString &button) {
                         executor.press(preset, button);
                         dashboard.setDeviceState(executor.state);
                     });
}
} // namespace
void schedule_smoke(manager::presentation::TrayDashboard &dashboard, Executor &executor) {
    QTimer::singleShot(0, &dashboard, [&dashboard, &executor] {
        auto *eco = dashboard.findChild<QPushButton *>(QStringLiteral("starter-min"));
        auto *fast = dashboard.findChild<QPushButton *>(QStringLiteral("starter-max"));
        if (!eco || !fast) {
            QCoreApplication::exit(1);
            return;
        }
        eco->click();
        if (executor.state.profileIndex != 1 || executor.state.refreshHz != 60) {
            QCoreApplication::exit(1);
            return;
        }
        fast->click();
        QCoreApplication::exit(executor.state.profileIndex == 2 && executor.state.refreshHz == 165 ? 0 : 1);
    });
}
int run_preset_preview(QApplication &app, bool smoke) {
    Session session;
    manager::presentation::RuntimeMonitor runtime(session);
    runtime.refresh();
    manager::presentation::TrayDashboard dashboard(runtime);
    Executor executor;
    initialize(executor);
    dashboard.setWindowFlags(Qt::Window);
    dashboard.setWindowTitle(QObject::tr("Tray preview — hardware is not changed"));
    dashboard.setDeviceState(executor.state);
    bind(dashboard, executor);
    dashboard.show();
    if (smoke)
        schedule_smoke(dashboard, executor);
    return app.exec();
}
