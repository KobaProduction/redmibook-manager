#include "adapters/storage/ui_preferences/store.hpp"
#include "apps/desktop/preview_presets.hpp"
#include "presentation/osd/osd_presenter.hpp"
#include "presentation/tray/dashboard.hpp"
#include "tests/fixtures/preset_preview.hpp"
#include <QApplication>
#include <QDir>
#include <QTemporaryDir>
#include <QTextStream>
#include <array>

namespace {
class PreviewSession final : public manager::application::SessionPort {
  public:
    bool request(manager::application::SessionCommand,
                 std::function<void(manager::application::SessionReply)> completion) override {
        completion({true,
                    {true, MANAGER_APP_VERSION, MANAGER_BUILD_ID, 1},
                    2,
                    MANAGER_APP_VERSION,
                    MANAGER_BUILD_ID});
        return true;
    }
};
bool capture(QWidget &widget, const QDir &directory, const QString &name) {
    widget.ensurePolished();
    widget.adjustSize();
    return widget.grab().save(directory.filePath(name + QStringLiteral(".png")));
}
bool render_modes(manager::presentation::OsdPresenter &osd, const QDir &directory) {
    using namespace manager::application::feedback;
    const std::array events{Event{Kind::Microphone, 0},  Event{Kind::Microphone, 1},
                            Event{Kind::Performance, 1}, Event{Kind::Performance, 2},
                            Event{Kind::Performance, 3}, Event{Kind::Refresh, 60},
                            Event{Kind::Refresh, 120}};
    for (const auto event : events) {
        if (osd.present(event) != Delivery::Presented ||
            !capture(osd, directory,
                     QStringLiteral("osd-%1-%2").arg(static_cast<int>(event.kind)).arg(event.value)))
            return false;
    }
    return true;
}
bool render_osd(manager::presentation::Preferences &preferences, const QDir &directory) {
    manager::presentation::OsdPresenter osd(preferences);
    if (!render_modes(osd, directory))
        return false;
    osd.show_result({true, {}, {}, 0});
    if (!capture(osd, directory, QStringLiteral("osd-test")))
        return false;
    osd.present_mi();
    if (!capture(osd, directory, QStringLiteral("osd-mi")))
        return false;
    osd.present_refresh({manager::application::display::Status::Unconfirmed, 0, {}});
    return capture(osd, directory, QStringLiteral("osd-unconfirmed"));
}
manager::presentation::TrayDeviceState tray_fixture() {
    manager::presentation::TrayDeviceState state;
    state.profileIndex = 0;
    state.profileAvailable = state.refreshAvailable = state.microphoneAvailable = true;
    state.refreshHz = 120;
    state.refreshRates = {60, 120};
    state.chargeLimit = 80;
    state.batteryRateW = -29.8;
    state.batteryDischarging = true;
    return state;
}
bool render_ready_variants(manager::presentation::TrayDashboard &dashboard,
                           manager::presentation::TrayDeviceState &state, const QDir &directory) {
    dashboard.setDeviceState(state);
    if (!capture(dashboard, directory, QStringLiteral("tray-ready")))
        return false;
    if (!capture(dashboard, directory, QStringLiteral("tray-discharging")))
        return false;
    state.batteryRateW = 35.2;
    state.batteryDischarging = false;
    dashboard.setDeviceState(state);
    if (!capture(dashboard, directory, QStringLiteral("tray-charging")))
        return false;
    state.presets = manager::preview::demo_presets();
    dashboard.setDeviceState(state);
    if (!capture(dashboard, directory, QStringLiteral("tray-presets")))
        return false;
    return true;
}
bool render_pending_variants(manager::presentation::TrayDashboard &dashboard,
                             manager::presentation::TrayDeviceState &state, const QDir &directory) {
    state.presets.clear();
    state.batteryRateW = 0;
    dashboard.setDeviceState(state);
    if (!capture(dashboard, directory, QStringLiteral("tray-zero-power")))
        return false;
    state.batteryRateW = 29.8;
    state.pending = true;
    state.microphoneMuted = true;
    dashboard.setDeviceState(state);
    if (!capture(dashboard, directory, QStringLiteral("tray-pending-muted")))
        return false;
    state.sections = {"refresh", "battery", "batteryPower", "connection"};
    dashboard.setDeviceState(state);
    if (!capture(dashboard, directory, QStringLiteral("tray-configured")))
        return false;
    return true;
}
bool render_connection(manager::presentation::TrayDashboard &dashboard,
                       manager::presentation::RuntimeMonitor &runtime,
                       manager::presentation::TrayDeviceState &state, const QDir &directory) {
    runtime.accept({});
    dashboard.setPreferenceConnection(
        false, QCoreApplication::translate(
                   "manager::presentation::Preferences",
                   "Background runtime is unavailable. Controls are disabled until reconnection."));
    state.pending = false;
    state.presets = manager::preview::demo_presets();
    dashboard.setDeviceState(state);
    auto *mode = dashboard.findChild<QToolButton *>(QStringLiteral("performanceMode0"));
    auto *presets = dashboard.findChild<manager::presentation::PresetPanel *>();
    if (!mode || mode->isEnabled() || !presets || presets->isEnabled() ||
        !capture(dashboard, directory, QStringLiteral("tray-offline")))
        return false;
    dashboard.setPreferenceConnection(true, {});
    if (!mode->isEnabled() || !presets->isEnabled())
        return false;
    return true;
}
bool render_tray(manager::presentation::Preferences &preferences, const QDir &directory) {
    PreviewSession session;
    manager::presentation::RuntimeMonitor runtime(session);
    runtime.refresh();
    manager::presentation::TrayDashboard dashboard(runtime);
    dashboard.setPalette(preferences.palette());
    auto state = tray_fixture();
    return render_ready_variants(dashboard, state, directory) &&
           render_pending_variants(dashboard, state, directory) &&
           render_connection(dashboard, runtime, state, directory);
}
bool render_theme(manager::presentation::Preferences &preferences, const QDir &root, const QString &theme) {
    if (!preferences.set_theme(theme) || !root.mkpath(theme))
        return false;
    // Only flush this offscreen fixture's palette/layout events; no native operations exist here.
    QCoreApplication::processEvents();
    const QDir directory(root.filePath(theme));
    return render_osd(preferences, directory) && render_tray(preferences, directory);
}
int render_all(const QString &path) {
    QTemporaryDir temporary;
    manager::adapters::UiPreferenceStore store(temporary.path());
    if (!store.update("language", "ru") || !store.update("osdSound", "false"))
        return 1;
    manager::presentation::Preferences preferences(store);
    const QDir output(path);
    const bool passed = render_theme(preferences, output, QStringLiteral("dark")) &&
                        render_theme(preferences, output, QStringLiteral("light"));
    QTextStream(stdout) << "Offscreen presentation fixtures; no hardware access; output: "
                        << output.absolutePath() << Qt::endl;
    return passed ? 0 : 1;
}
} // namespace
int main(int argc, char *argv[]) {
    const bool interactive = argc == 2 && std::string_view(argv[1]) == "--interactive-presets";
    const bool preset_smoke = argc == 2 && std::string_view(argv[1]) == "--preset-smoke";
    // Rendering never attaches to the desktop; interactive mode requires an explicit launch.
    if (!interactive)
        qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);
    app.setStyle(QStringLiteral("Fusion"));
    if (app.arguments().size() != 2)
        return 2;
    if (interactive || preset_smoke)
        return run_preset_preview(app, preset_smoke);
    return render_all(app.arguments().at(1));
}
