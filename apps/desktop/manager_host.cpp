#include "apps/desktop/manager_host.hpp"
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
      update_source_(adapters::update_target()), updates_(update_source_, adapters::update_target()) {
    preferences_.attach_engine(engine_);
    wire_manager_appearance(preferences_, model_, engine_);
    QObject::connect(&preferences_, &presentation::Preferences::languageChanged, &updates_,
                     &presentation::UpdateModel::changed);
    QObject::connect(
        &engine_, &QQmlApplicationEngine::objectCreationFailed, QCoreApplication::instance(),
        [] { QCoreApplication::exit(1); }, Qt::QueuedConnection);
    engine_.setInitialProperties({{QStringLiteral("about"), QVariant::fromValue(&about_)},
                                  {QStringLiteral("manager"), QVariant::fromValue(&model_)},
                                  {QStringLiteral("updates"), QVariant::fromValue(&updates_)}});
}

namespace {
void show_manager(const QQmlApplicationEngine &engine) {
    if (QCoreApplication::arguments().contains(QStringLiteral("--settings"))) {
        engine.rootObjects().first()->setProperty("page", 3);
    }
    if (auto *window = qobject_cast<QWindow *>(engine.rootObjects().first())) {
        presentation::present_window(*window, QCursor::pos());
    }
}
} // namespace

int ManagerHost::open(bool smoke, const QString &screenshot, bool ui_contract) {
    if (!smoke && !instance_.acquire(engine_)) {
        return 0;
    }
    if (!smoke && !restored_) {
        adapters::ensure_session();
    }
    engine_.loadFromModule(QStringLiteral("Redmibook.Manager"), QStringLiteral("Main"));
    if (engine_.rootObjects().isEmpty()) {
        return 1;
    }
    if (!smoke) {
        show_manager(engine_);
    }
    if (ui_contract) {
        schedule_ui_contract(engine_, model_, screenshot);
    } else if (smoke) {
        schedule_smoke_test(engine_, screenshot);
    }
    return -1;
}
} // namespace manager::desktop
