#include "apps/desktop/manager_host.hpp"
#include "adapters/platform/firmware_controls.hpp"
#include "adapters/updates/target.hpp"
#include "apps/desktop/appearance_setup.hpp"
#include "apps/desktop/smoke_test.hpp"
#include "apps/desktop/ui_contract_smoke.hpp"
#include "presentation/common/displays.hpp"
#include <QCoreApplication>
#include <QCursor>
#include <QWindow>

namespace manager::desktop {
ManagerHost::ManagerHost(bool smoke)
    : store_(smoke ? temporary_.path() : adapters::preferences_directory()), preferences_(store_),
      model_(preferences_, client_), about_(profile_source_, preferences_),
      controls_(microphone_, refresh_, preferences_, adapters::make_core_charge_control(),
                adapters::make_core_performance_control()),
      update_source_(adapters::update_target()), updates_(update_source_, adapters::update_target()) {
    preferences_.attach_engine(engine_);
    wire_manager_appearance(preferences_, model_, engine_);
    wire_device_state(smoke);
    QObject::connect(&preferences_, &presentation::Preferences::languageChanged, &updates_,
                     &presentation::UpdateModel::changed);
    QObject::connect(
        &engine_, &QQmlApplicationEngine::objectCreationFailed, QCoreApplication::instance(),
        [] { QCoreApplication::exit(1); }, Qt::QueuedConnection);
    engine_.setInitialProperties({{QStringLiteral("about"), QVariant::fromValue(&about_)},
                                  {QStringLiteral("controls"), QVariant::fromValue(&controls_)},
                                  {QStringLiteral("manager"), QVariant::fromValue(&model_)},
                                  {QStringLiteral("updates"), QVariant::fromValue(&updates_)}});
}

void ManagerHost::wire_device_state(bool smoke) {
    if (smoke)
        return;
    QObject::connect(&state_client_, &adapters::DeviceStateClient::stateChanged, &controls_,
                     &presentation::DeviceControlsModel::acceptState);
    QObject::connect(&state_client_, &adapters::DeviceStateClient::unavailable, &controls_,
                     [this] { controls_.acceptState({}); });
    QObject::connect(&controls_, &presentation::DeviceControlsModel::stateRefreshRequested, &state_client_,
                     &adapters::DeviceStateClient::refresh);
    QObject::connect(&model_, &presentation::ManagerModel::changed, &state_client_,
                     &adapters::DeviceStateClient::start);
    state_client_.start();
}

namespace {
void show_manager(const QQmlApplicationEngine &engine, ManagerActivation activation) {
    if (activation != ManagerActivation::Overview &&
        QCoreApplication::arguments().contains(QStringLiteral("--settings"))) {
        engine.rootObjects().first()->setProperty("page", 4);
    }
    if (auto *window = qobject_cast<QWindow *>(engine.rootObjects().first())) {
        presentation::present_window(*window, QCursor::pos());
    }
}
} // namespace

int ManagerHost::open(bool smoke, const QString &screenshot, bool ui_contract, ManagerActivation activation) {
    if (!smoke && !instance_.acquire(engine_, activation)) {
        return 0;
    }
    if (!smoke && !restored_) {
        adapters::ensure_session();
    }
    engine_.loadFromModule(QStringLiteral("Redmibook.Manager"), QStringLiteral("Main"));
    if (engine_.rootObjects().isEmpty()) {
        return 1;
    }
    if (activation == ManagerActivation::Overview)
        engine_.rootObjects().first()->setProperty("page", 0);
    if (!smoke) {
        show_manager(engine_, activation);
    }
    if (ui_contract) {
        schedule_ui_contract(engine_, model_, screenshot);
    } else if (smoke) {
        schedule_smoke_test(engine_, screenshot);
    }
    return -1;
}
} // namespace manager::desktop
