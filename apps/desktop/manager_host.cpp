#include "apps/desktop/manager_host.hpp"
#include "adapters/local_api/feedback_channel.hpp"
#include "adapters/platform/firmware_controls.hpp"
#include "adapters/platform/power_controls.hpp"
#include "adapters/platform/power_scheme.hpp"
#include "adapters/platform/window_appearance.hpp"
#include "adapters/updates/target.hpp"
#include "apps/desktop/appearance_setup.hpp"
#include "apps/desktop/preview_presets.hpp"
#include "apps/desktop/smoke_test.hpp"
#include "apps/desktop/style_setup.hpp"
#include "apps/desktop/ui_contract_smoke.hpp"
#include "presentation/common/displays.hpp"
#include <QCoreApplication>
#include <QCursor>
#include <QTimer>
#include <QWindow>

namespace manager::desktop {
ManagerHost::ManagerHost(bool smoke, std::unique_ptr<application::PreferencePort> store)
    : store_(store ? std::move(store) : std::make_unique<adapters::UiPreferenceStore>(temporary_.path())),
      preferences_(*store_), model_(preferences_, client_), about_(profile_source_, preferences_),
      controls_(microphone_, refresh_, preferences_, adapters::make_core_charge_control(),
                adapters::make_core_performance_control(),
                smoke ? nullptr : std::make_shared<adapters::FeedbackClient>(),
                smoke ? nullptr : adapters::make_core_microphone_state()),
      brightness_(smoke ? nullptr : adapters::make_brightness()),
      saver_(smoke ? nullptr : adapters::make_energy_saver()), power_controls_(brightness_, saver_),
      preset_executor_(std::make_shared<adapters::PresetExecutor>(adapters::make_core_performance_control(),
                                                                  refresh_, adapters::make_power_schemes(),
                                                                  brightness_, saver_)),
      presets_(preferences_,
               [executor = preset_executor_, smoke] {
                   if (!smoke)
                       return executor->choices();
                   return preview::demo_catalog();
               }),
      update_source_(adapters::update_target()), updates_(update_source_, adapters::update_target()) {
    if (smoke && QCoreApplication::arguments().contains(QStringLiteral("--preset-preview")))
        if (auto *fixture = dynamic_cast<adapters::UiPreferenceStore *>(store_.get()))
            fixture->configure_starter_defaults(preview::demo_catalog());
    if (smoke)
        power_controls_.setPreviewState(70, false);
    wire_preferences(smoke);
    QObject::connect(
        &engine_, &QQmlApplicationEngine::objectCreationFailed, QCoreApplication::instance(),
        [] { QCoreApplication::exit(1); }, Qt::QueuedConnection);
    engine_.setInitialProperties(
        {{QStringLiteral("preferenceConnection"), QVariant::fromValue(&preferences_)},
         {QStringLiteral("about"), QVariant::fromValue(&about_)},
         {QStringLiteral("powerControls"), QVariant::fromValue(&power_controls_)},
         {QStringLiteral("presets"), QVariant::fromValue(&presets_)},
         {QStringLiteral("controls"), QVariant::fromValue(&controls_)},
         {QStringLiteral("manager"), QVariant::fromValue(&model_)},
         {QStringLiteral("updates"), QVariant::fromValue(&updates_)}});
}

void ManagerHost::wire_preferences(bool smoke) {
    preferences_.attach_engine(engine_);
    QObject::connect(&preferences_, &presentation::Preferences::changed, &engine_,
                     [this] { load_content(); });
    QObject::connect(&preferences_, &presentation::Preferences::reconnectRequested, &preferences_, [smoke] {
        if (!smoke)
            adapters::ensure_session();
    });
    QObject::connect(&controls_, &presentation::DeviceControlsModel::settingsResetReady, &model_,
                     &presentation::ManagerModel::resetSettings);
    wire_manager_appearance(preferences_, model_, engine_);
    wire_device_state(smoke);
    QObject::connect(&preferences_, &presentation::Preferences::languageChanged, &updates_,
                     &presentation::UpdateModel::changed);
}

void ManagerHost::load_content() {
    if (content_started_ || !preferences_.available() || engine_.rootObjects().isEmpty())
        return;
    configure_controls(preferences_.design());
    content_started_ = true;
    QMetaObject::invokeMethod(engine_.rootObjects().first(), "loadContent");
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
    QObject::connect(&controls_, &presentation::DeviceControlsModel::batteryTelemetryRefreshRequested,
                     &state_client_, &adapters::DeviceStateClient::refreshBattery);
    QObject::connect(&controls_, &presentation::DeviceControlsModel::batteryWatchRequested, &state_client_,
                     &adapters::DeviceStateClient::watchBattery);
    QObject::connect(&state_client_, &adapters::DeviceStateClient::refreshStarted, &controls_,
                     &presentation::DeviceControlsModel::beginRefresh);
    QObject::connect(&state_client_, &adapters::DeviceStateClient::refreshFinished, &controls_,
                     &presentation::DeviceControlsModel::finishRefresh);
    QObject::connect(&model_, &presentation::ManagerModel::changed, &state_client_,
                     &adapters::DeviceStateClient::start);
    state_client_.start();
}

namespace {
void after_content_ready(QQmlApplicationEngine &engine, const std::function<void()> &run) {
    auto *root = engine.rootObjects().first();
    if (root->property("contentReady").toBool() ||
        QCoreApplication::arguments().contains(QStringLiteral("--loading-preview"))) {
        run();
        return;
    }
    auto *ready = new QTimer(&engine);
    ready->setSingleShot(true);
    QObject::connect(root, SIGNAL(contentLoaded()), ready, SLOT(start()));
    QObject::connect(ready, &QTimer::timeout, &engine, [ready, run] {
        ready->deleteLater();
        run();
    });
}
void show_manager(const QQmlApplicationEngine &engine, ManagerActivation activation) {
    if (activation != ManagerActivation::Overview &&
        QCoreApplication::arguments().contains(QStringLiteral("--settings"))) {
        engine.rootObjects().first()->setProperty("page", 4);
    }
    if (auto *window = qobject_cast<QWindow *>(engine.rootObjects().first())) {
        presentation::present_window(*window, QCursor::pos(), [window] {
            adapters::request_window_foreground(static_cast<std::uintptr_t>(window->winId()));
        });
    }
}
} // namespace

void ManagerHost::present_when_ready(ManagerActivation activation) {
    const auto present = [this, activation] {
        auto *root = engine_.rootObjects().first();
        if (root->property("startupPresented").toBool() || preferences_.connecting())
            return;
        root->setProperty("startupPresented", true);
        show_manager(engine_, activation);
    };
    // Snapshot delivery applies language/palette before this queued presentation.
    QObject::connect(&preferences_, &presentation::Preferences::changed, &engine_, present,
                     Qt::QueuedConnection);
    QObject::connect(&preferences_, &presentation::Preferences::connectionChanged, &engine_, present,
                     Qt::QueuedConnection);
    present();
}

void ManagerHost::schedule_preview(bool smoke, const QString &screenshot, bool ui_contract,
                                   bool preset_preview) {
    if (!smoke)
        return;
    const auto run = [this, screenshot, ui_contract, preset_preview] {
        if (preset_preview)
            schedule_preset_preview(engine_, model_, screenshot);
        else if (ui_contract)
            schedule_ui_contract(engine_, model_, screenshot);
        else
            schedule_smoke_test(engine_, screenshot);
    };
    after_content_ready(engine_, run);
}

void ManagerHost::present(bool smoke, ManagerActivation activation) {
    if (smoke) {
        engine_.rootObjects().first()->setProperty("startupPresented", true);
        return;
    }
    present_when_ready(activation);
    if (!restored_)
        QTimer::singleShot(0, &preferences_, [] { adapters::ensure_session(); });
}

int ManagerHost::open(bool smoke, const QString &screenshot, bool ui_contract, ManagerActivation activation,
                      bool preset_preview) {
    if (!smoke && !instance_.acquire(engine_, activation)) {
        return 0;
    }
    engine_.loadFromModule(QStringLiteral("Redmibook.Manager"), QStringLiteral("Main"));
    if (engine_.rootObjects().isEmpty()) {
        return 1;
    }
    if (activation == ManagerActivation::Overview)
        engine_.rootObjects().first()->setProperty("page", 0);
    load_content();
    present(smoke, activation);
    schedule_preview(smoke, screenshot, ui_contract, preset_preview);
    return -1;
}
} // namespace manager::desktop
