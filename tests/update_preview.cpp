#include "adapters/device_profile/demo_profile.hpp"
#include "adapters/storage/ui_preferences/store.hpp"
#include "presentation/about_model.hpp"
#include "presentation/manager_model.hpp"
#include "presentation/updates/update_model.hpp"
#include <QApplication>
#include <QDir>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QTemporaryDir>
#include <QTimer>

namespace {
namespace application = manager::application;
class Session final : public application::SessionPort {
  public:
    bool request(application::SessionCommand,
                 std::function<void(application::SessionReply)> complete) override {
        complete({});
        return true;
    }
};
class Feed final : public application::UpdatePort {
  public:
    void check(std::function<void(application::UpdateResult)> complete) override {
        complete({.state = application::UpdateState::Available,
                  .version = "1.2.3",
                  .download_url = "preview",
                  .download_sha256 = "preview",
                  .download_size = 100});
    }
};
class Packages final : public application::UpdatePackagePort {
  public:
    Progress progress;
    std::function<void(bool)> complete;
    void download(const application::UpdateResult &, Progress update,
                  std::function<void(bool)> done) override {
        progress = std::move(update);
        complete = std::move(done);
    }
    void install(std::function<void(application::InstallResult)> done) override {
        done(application::InstallResult::Cancelled);
    }
};
struct Preview {
    QTemporaryDir temporary;
    manager::adapters::UiPreferenceStore store{temporary.path()};
    manager::presentation::Preferences preferences{store};
    Session session;
    manager::presentation::ManagerModel manager{preferences, session};
    manager::presentation::AboutModel about{manager::adapters::DemoDeviceProfile{}.read(), preferences};
    Feed feed;
    Packages packages;
    manager::presentation::UpdateModel updates{feed, QStringLiteral("windows-x86_64"), &packages};
    QQmlApplicationEngine engine;
    Preview() {
        preferences.set_language(QStringLiteral("ru"));
        preferences.set_theme(QStringLiteral("dark"));
        preferences.attach_engine(engine);
        updates.check();
        engine.setInitialProperties({{"manager", QVariant::fromValue(&manager)},
                                     {"updates", QVariant::fromValue(&updates)},
                                     {"about", QVariant::fromValue(&about)}});
        engine.loadData(R"QML(
import QtQuick
import Redmibook.Manager
Window {
    id: root
    required property ManagerModel manager
    required property UpdateModel updates
    required property AboutModel about
    width: 980; height: 860; visible: true
    palette: AppPalette {}
    color: palette.window
    AboutPage {
        anchors.fill: parent; anchors.margins: 24
        manager: root.manager; updates: root.updates; about: root.about
    }
})QML");
    }
};
void capture(Preview &preview, const QString &directory, int step) {
    auto *window = qobject_cast<QQuickWindow *>(preview.engine.rootObjects().first());
    const QStringList names{"available.png", "downloading.png", "install.png"};
    if (!window || !window->grabWindow().save(QDir(directory).filePath(names.at(step)))) {
        QCoreApplication::exit(1);
        return;
    }
    if (step == 0) {
        preview.updates.download();
        preview.packages.progress(50, 100);
    } else if (step == 1) {
        preview.packages.complete(true);
    } else {
        QCoreApplication::exit(0);
        return;
    }
    QTimer::singleShot(200, &preview.engine,
                       [&preview, directory, step] { capture(preview, directory, step + 1); });
}
} // namespace
int run_update_preview(QApplication &app, const QString &directory) {
    QQuickStyle::setStyle(QStringLiteral("FluentWinUI3"));
    QDir().mkpath(directory);
    Preview preview;
    if (preview.engine.rootObjects().isEmpty())
        return 1;
    QTimer::singleShot(500, &preview.engine, [&preview, directory] { capture(preview, directory, 0); });
    QTimer::singleShot(5000, &preview.engine, [] { QCoreApplication::exit(2); });
    return app.exec();
}
