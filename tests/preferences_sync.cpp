#include "tests/preferences_sync.hpp"
#include "adapters/storage/ui_preferences/store.hpp"
#include "presentation/tray/tray_client.hpp"
#include <QElapsedTimer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QTemporaryDir>
#include <QTextStream>
#include <QTimer>

namespace {
class SessionFake final : public manager::application::SessionPort {
  public:
    bool request(manager::application::SessionCommand,
                 std::function<void(manager::application::SessionReply)> completion) override {
        completion({});
        return true;
    }
};
QJsonObject snapshot(const manager::presentation::Preferences &preferences) {
    for (auto *widget : QApplication::topLevelWidgets()) {
        if (widget->objectName() == QStringLiteral("trayDashboard")) {
            const auto *mode = widget->findChild<QToolButton *>(QStringLiteral("performanceMode1"));
            return {{QStringLiteral("language"), preferences.language()},
                    {QStringLiteral("position"), preferences.osd_position()},
                    {QStringLiteral("autostart"), preferences.tray_autostart()},
                    {QStringLiteral("mode"), mode == nullptr ? QString() : mode->text()}};
        }
    }
    return {};
}

bool matches_snapshot(const QJsonObject &value, const QString &language) {
    const bool russian = language == QStringLiteral("ru");
    return value.value(QStringLiteral("language")) == language &&
           value.value(QStringLiteral("position")) ==
               (russian ? QStringLiteral("bottom-right") : QStringLiteral("center")) &&
           value.value(QStringLiteral("autostart")).toBool() != russian &&
           value.value(QStringLiteral("mode")) ==
               (russian ? QStringLiteral("Тихий") : QStringLiteral("Quiet"));
}
bool await_snapshot(QProcess &child, const QString &language) {
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < 5000) {
        child.waitForReadyRead(100);
        while (child.canReadLine()) {
            if (matches_snapshot(QJsonDocument::fromJson(child.readLine()).object(), language)) {
                return true;
            }
        }
    }
    return false;
}
bool update_snapshot(manager::adapters::UiPreferenceStore &writer, bool russian) {
    return writer.update("language", russian ? "ru" : "en") &&
           writer.update("osdPosition", russian ? "bottom-right" : "center") &&
           writer.update("trayAutostart", russian ? "false" : "true");
}
} // namespace

int observe_preferences(QApplication &app, const QString &directory) {
    manager::adapters::UiPreferenceStore store(directory);
    manager::presentation::Preferences preferences(store);
    SessionFake session;
    manager::presentation::TrayClient tray(preferences, session);
    const auto publish = [&preferences] {
        QTextStream output(stdout);
        output << QJsonDocument(snapshot(preferences)).toJson(QJsonDocument::Compact) << '\n';
        output.flush();
    };
    QObject::connect(&preferences, &manager::presentation::Preferences::changed, &app, publish);
    publish();
    QTimer::singleShot(12000, &app, &QCoreApplication::quit);
    return app.exec();
}

bool preferences_sync_contract() {
    QTemporaryDir directory;
    manager::adapters::UiPreferenceStore writer(directory.path());
    if (!writer.update("language", "en")) {
        return false;
    }
    QProcess child;
    child.start(QCoreApplication::applicationFilePath(), {QStringLiteral("--observe"), directory.path()});
    if (!child.waitForStarted(2000) || !child.waitForReadyRead(3000)) {
        return false;
    }
    child.readAllStandardOutput();
    const bool passed = update_snapshot(writer, true) && await_snapshot(child, QStringLiteral("ru")) &&
                        update_snapshot(writer, false) && await_snapshot(child, QStringLiteral("en"));
    child.kill();
    child.waitForFinished(2000);
    return passed;
}
