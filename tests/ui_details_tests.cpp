#include "adapters/device_profile/demo_profile.hpp"
#include "adapters/storage/ui_preferences/store.hpp"
#include "presentation/about_model.hpp"
#include "presentation/manager_model.hpp"
#include "presentation/osd/diagnostic_osd.hpp"
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
    manager::presentation::DiagnosticOsd osd(preferences);
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
    return licenses(preferences) && rounding(preferences) && design(preferences) ? 0 : 1;
}
