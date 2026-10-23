#include "adapters/device_profile/demo_profile.hpp"
#include "adapters/storage/ui_preferences/store.hpp"
#include "presentation/about_model.hpp"
#include "presentation/device_controls_model.hpp"
#include "presentation/manager_model.hpp"
#include "presentation/osd/osd_presenter.hpp"
#include <QApplication>
#include <QTemporaryDir>
namespace {
class SessionFake final : public manager::application::SessionPort {
  public:
    bool request(manager::application::SessionCommand,
                 std::function<void(manager::application::SessionReply)> complete) override {
        complete({});
        return true;
    }
};
bool licenses(manager::presentation::Preferences &preferences) {
    manager::adapters::DemoDeviceProfile source;
    manager::presentation::AboutModel model(source, preferences);
    const auto original = model.licenseText(0, true);
    const auto qt = model.licenseText(1, true);
    const auto notices = model.licenseText(2, true);
    if (!model.demo() || !original.startsWith(QStringLiteral("MIT License")) || qt.size() < 6000 ||
        notices.size() < 500) {
        return false;
    }
    if (!preferences.set_language(QStringLiteral("ru"))) {
        return false;
    }
    const bool translated = model.mitTranslation().startsWith(QStringLiteral("Лицензия MIT")) &&
                            model.hasTranslation() && model.licenseText(0, true) == original;
    return preferences.set_language(QStringLiteral("en")) && translated && !model.hasTranslation();
}
bool rounding(manager::presentation::Preferences &preferences) {
    manager::presentation::OsdPresenter osd(preferences);
    osd.show_result({});
    QCoreApplication::processEvents();
    const auto rounded = osd.grab().toImage().pixelColor(0, 0).alpha();
    if (!preferences.set_rounded_corners(false)) {
        return false;
    }
    QCoreApplication::processEvents();
    const auto square = osd.grab().toImage().pixelColor(0, 0).alpha();
    return rounded == 0 && square > 0;
}
bool design(manager::presentation::Preferences &preferences) {
    SessionFake runtime;
    manager::presentation::ManagerModel model(preferences, runtime);
    bool restart = false;
    QObject::connect(&model, &manager::presentation::ManagerModel::restartRequested,
                     [&restart] { restart = true; });
    model.setDesign(1);
    if (!model.designPending() || preferences.design() != QStringLiteral("fluent")) {
        return false;
    }
    model.applyDesign();
    manager::presentation::ManagerModel reopened(preferences, runtime);
    return restart && !reopened.designPending() && reopened.designIndex() == 1;
}
bool reset_classic_design(manager::presentation::Preferences &preferences) {
    if (!preferences.set_design(QStringLiteral("classic")))
        return false;
    SessionFake runtime;
    manager::presentation::ManagerModel model(preferences, runtime);
    model.resetSettings();
    if (!model.designPending() || preferences.design() != QStringLiteral("fluent"))
        return false;
    int restarts = 0;
    QObject::connect(&model, &manager::presentation::ManagerModel::restartRequested, [&] { ++restarts; });
    model.applyDesign();
    return restarts == 1;
}
} // namespace
namespace {
bool battery_preferences() {
    QTemporaryDir directory;
    manager::adapters::UiPreferenceStore store(directory.path());
    manager::presentation::Preferences preferences(store);
    if (preferences.battery_interval_ms() != 1000 || !preferences.set_battery_interval_ms(5000) ||
        !preferences.set_tray_section_enabled(QStringLiteral("refresh"), false))
        return false;
    const auto saved = store.read();
    if (!saved || saved->battery_interval_ms != 5000 || preferences.tray_section_enabled("refresh"))
        return false;
    if (store.update("batteryIntervalMs", "5") || store.update("batteryIntervalMs", "7") ||
        store.update("traySections", "[\"unknown\"]"))
        return false;
    const auto after = store.read();
    return after && after->battery_interval_ms == 5000;
}
bool telemetry_notifications(manager::presentation::Preferences &preferences) {
    class Microphone final : public manager::application::microphone::ControlPort {
      public:
        manager::application::microphone::Observation read() const override {
            return {};
        }
        manager::application::microphone::Observation set_muted(bool) const override {
            return {};
        }
    };
    class Display final : public manager::application::display::RefreshPort {
      public:
        manager::application::display::Snapshot read() const override {
            return {};
        }
        manager::application::display::Change set_refresh(const std::string &, unsigned int) const override {
            return {};
        }
    };
    manager::presentation::DeviceControlsModel model(std::make_shared<Microphone>(),
                                                     std::make_shared<Display>(), preferences);
    int global{}, battery{};
    QObject::connect(&model, &manager::presentation::DeviceControlsModel::changed, &model, [&] { ++global; });
    QObject::connect(&model, &manager::presentation::DeviceControlsModel::batteryTelemetryChanged, &model,
                     [&] { ++battery; });
    manager::application::overview::DeviceState state;
    state.battery_telemetry.status = manager::application::battery::TelemetryStatus::Available;
    state.battery_telemetry.voltage = 16.1;
    model.acceptState(state);
    model.acceptState(state);
    state.battery_telemetry.voltage = 16.2;
    model.acceptState(state);
    return global == 0 && battery == 2;
}
} // namespace
int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    app.setStyle(QStringLiteral("Fusion"));
    QTemporaryDir directory;
    manager::adapters::UiPreferenceStore store(directory.path());
    if (!store.update("language", "en")) {
        return 1;
    }
    manager::presentation::Preferences preferences(store);
    return licenses(preferences) && rounding(preferences) && design(preferences) && battery_preferences() &&
                   telemetry_notifications(preferences) && reset_classic_design(preferences)
               ? 0
               : 1;
}
