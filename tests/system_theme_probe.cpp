#include "adapters/device_profile/demo_profile.hpp"
#include "adapters/storage/ui_preferences/store.hpp"
#include "presentation/about_model.hpp"
#include "presentation/manager_model.hpp"
#include "presentation/updates/update_model.hpp"
#include <QApplication>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QStyleHints>
#include <QTemporaryDir>
#include <QTextStream>
#include <QTimer>
namespace {
class SessionFake final : public manager::application::SessionPort {
  public:
    bool request(manager::application::SessionCommand,
                 std::function<void(manager::application::SessionReply)> complete) override {
        complete({});
        return true;
    }
};
class UpdatesFake final : public manager::application::UpdatePort {
  public:
    void check(std::function<void(manager::application::UpdateResult)> complete) override {
        complete({});
    }
};
bool rendered(QQmlApplicationEngine &engine, bool dark) {
    auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
    const auto image = window->grabWindow();
    if (image.isNull()) {
        return false;
    }
    const auto color = image.pixelColor(image.width() / 2, image.height() - 3);
    QTextStream(stdout) << "System mode render: " << color.name() << " expected dark=" << dark << Qt::endl;
    return (color.lightness() < 128) == dark &&
           (QApplication::palette().color(QPalette::Window).lightness() < 128) == dark;
}
void probe(QQmlApplicationEngine &engine, manager::presentation::ManagerModel &model) {
    QGuiApplication::styleHints()->setColorScheme(Qt::ColorScheme::Dark);
    QTimer::singleShot(350, &engine, [&engine, &model] {
        if (model.themeIndex() != 0 || !rendered(engine, true)) {
            QCoreApplication::exit(1);
            return;
        }
        QGuiApplication::styleHints()->setColorScheme(Qt::ColorScheme::Light);
        QTimer::singleShot(350, &engine, [&engine, &model] {
            const bool valid = model.themeIndex() == 0 && rendered(engine, false);
            QGuiApplication::styleHints()->unsetColorScheme();
            QCoreApplication::exit(valid ? 0 : 1);
        });
    });
}
void schedule_probe(QQmlApplicationEngine &engine, manager::presentation::ManagerModel &model) {
    QTimer::singleShot(300, &engine, [&engine, &model] { probe(engine, model); });
    QTimer::singleShot(5000, &engine, [] { QCoreApplication::exit(2); });
}

int run(QApplication &app) {
    QTemporaryDir directory;
    manager::adapters::UiPreferenceStore store(directory.path());
    store.update("language", "en");
    manager::presentation::Preferences preferences(store);
    SessionFake session;
    manager::presentation::ManagerModel model(preferences, session);
    manager::adapters::DemoDeviceProfile profile;
    manager::presentation::AboutModel about(profile, preferences);
    UpdatesFake source;
    manager::presentation::UpdateModel updates(source, QStringLiteral("windows-x86_64"));
    QQmlApplicationEngine engine;
    engine.setInitialProperties({{QStringLiteral("manager"), QVariant::fromValue(&model)},
                                 {QStringLiteral("updates"), QVariant::fromValue(&updates)},
                                 {QStringLiteral("about"), QVariant::fromValue(&about)}});
    engine.loadFromModule(QStringLiteral("Redmibook.Manager"), QStringLiteral("Main"));
    if (engine.rootObjects().isEmpty()) {
        return 1;
    }
    schedule_probe(engine, model);
    return app.exec();
}
} // namespace
int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    app.setStyle(QStringLiteral("Fusion"));
    QQuickStyle::setStyle(QStringLiteral("FluentWinUI3"));
    if (QGuiApplication::platformName() != QStringLiteral("windows")) {
        return 77;
    }
    return run(app);
}
