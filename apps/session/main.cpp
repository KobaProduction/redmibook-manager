#include "adapters/local_api/session_client.hpp"
#include "adapters/local_api/session_server.hpp"
#include "adapters/platform/runtime.hpp"
#include "adapters/storage/ui_preferences/store.hpp"
#include "apps/session/update_scheduler.hpp"
#include "presentation/common/preferences.hpp"
#include "presentation/osd/diagnostic_osd.hpp"
#include <QApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLockFile>

namespace {
int request_status(QApplication &app, const QCommandLineParser &parser) {
    manager::adapters::SessionClient client;
    using manager::application::SessionCommand;
    const auto command =
        parser.isSet(QStringLiteral("shutdown")) ? SessionCommand::Shutdown : SessionCommand::Diagnostic;
    client.request(command, [&parser](const manager::application::SessionReply &reply) {
        if (parser.isSet(QStringLiteral("output"))) {
            QFile output(parser.value(QStringLiteral("output")));
            if (output.open(QIODevice::WriteOnly)) {
                const QJsonObject value{
                    {QStringLiteral("session"), reply.connected},
                    {QStringLiteral("daemon"), reply.daemon.connected},
                    {QStringLiteral("sessionPid"), static_cast<qint64>(reply.process_id)},
                    {QStringLiteral("daemonPid"), static_cast<qint64>(reply.daemon.process_id)},
                    {QStringLiteral("build"), QString::fromStdString(reply.daemon.build)}};
                output.write(QJsonDocument(value).toJson());
            }
        }
        QCoreApplication::exit(reply.connected ? 0 : 1);
    });
    return app.exec();
}

void configure(QCommandLineParser &parser) {
    parser.addHelpOption();
    parser.addOption({QStringLiteral("restored"), QStringLiteral("Restored by installer lifecycle")});
    parser.addOption({QStringLiteral("with-tray"), QStringLiteral("Open tray client")});
    parser.addOption({QStringLiteral("probe"), QStringLiteral("Send bounded diagnostic request")});
    parser.addOption({QStringLiteral("shutdown"), QStringLiteral("Stop session background runtime")});
    parser.addOption(
        {QStringLiteral("output"), QStringLiteral("Write diagnostic JSON"), QStringLiteral("path")});
}
} // namespace

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    app.setQuitOnLastWindowClosed(false);
    app.setStyle(QStringLiteral("Fusion"));
    manager::adapters::configure_gui_identity();
    QCommandLineParser parser;
    configure(parser);
    parser.process(app);
    if (parser.isSet(QStringLiteral("probe")) || parser.isSet(QStringLiteral("shutdown"))) {
        return request_status(app, parser);
    }
    manager::adapters::UiPreferenceStore store(manager::adapters::preferences_directory());
    manager::presentation::Preferences preferences(store);
    QLockFile lock(
        QDir(store.directory()).filePath(manager::adapters::session_endpoint() + QStringLiteral(".lock")));
    if (!lock.tryLock(2000)) {
        return 0;
    }
    const manager::adapters::NativeDiagnostic diagnostic;
    const manager::application::ReadRuntimeStatus read(diagnostic);
    manager::presentation::DiagnosticOsd osd(preferences);
    manager::session::UpdateScheduler updates(preferences, store.directory());
    manager::adapters::SessionServer server(read, osd);
    if (!server.listen()) {
        return 1;
    }
    QObject::connect(&server, &manager::adapters::SessionServer::openTray, &app,
                     [] { manager::adapters::launch_component(QStringLiteral("redmibook-tray")); });
    QObject::connect(&server, &manager::adapters::SessionServer::restartRequested, &app,
                     [] { manager::adapters::launch_component(QStringLiteral("redmibook-session")); });
    if (parser.isSet(QStringLiteral("with-tray")) && preferences.tray_autostart()) {
        manager::adapters::launch_component(QStringLiteral("redmibook-tray"));
    }
    return app.exec();
}
