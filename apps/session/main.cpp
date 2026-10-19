#include "adapters/local_api/session_client.hpp"
#include "adapters/local_api/session_server.hpp"
#include "adapters/platform/runtime.hpp"
#include "adapters/storage/ui_preferences/store.hpp"
#include "application/runtime/connection_health.hpp"
#include "apps/session/device_state_publisher.hpp"
#include "apps/session/input_actions.hpp"
#include "apps/session/update_scheduler.hpp"
#include "apps/session/vendor_actions.hpp"
#include "presentation/common/preferences.hpp"
#include "presentation/osd/diagnostic_osd.hpp"
#include <QApplication>
#include <QCommandLineParser>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLockFile>
#include <QTextStream>
#include <QTimer>
#include <cstdio>

namespace {
int request_status(QApplication &app, const QCommandLineParser &parser) {
    manager::adapters::SessionClient client;
    using manager::application::SessionCommand;
    const auto command =
        parser.isSet(QStringLiteral("shutdown")) ? SessionCommand::Shutdown : SessionCommand::Status;
    client.request(command, [&parser](const manager::application::SessionReply &reply) {
        if (parser.isSet(QStringLiteral("output"))) {
            QFile output(parser.value(QStringLiteral("output")));
            if (output.open(QIODevice::WriteOnly)) {
                const QJsonObject value{
                    {QStringLiteral("session"), reply.connected},
                    {QStringLiteral("daemon"), reply.daemon.connected},
                    {QStringLiteral("compatible"), manager::application::connection_health(
                                                       reply, {MANAGER_APP_VERSION, MANAGER_BUILD_ID}) ==
                                                       manager::application::ConnectionHealth::Ready},
                    {QStringLiteral("sessionVersion"), QString::fromStdString(reply.session_version)},
                    {QStringLiteral("sessionBuild"), QString::fromStdString(reply.session_build)},
                    {QStringLiteral("sessionPid"), static_cast<qint64>(reply.process_id)},
                    {QStringLiteral("daemonPid"), static_cast<qint64>(reply.daemon.process_id)},
                    {QStringLiteral("build"), QString::fromStdString(reply.daemon.build)}};
                output.write(QJsonDocument(value).toJson());
            }
        }
        const int exit_code = reply.connected ? 0 : 1;
        QTimer::singleShot(0, [exit_code] { QCoreApplication::exit(exit_code); });
    });
    return app.exec();
}

std::unique_ptr<manager::session::InputActions> session_input(manager::presentation::Preferences &preferences,
                                                              const QCommandLineParser &parser) {
    auto input = std::make_unique<manager::session::InputActions>(preferences);
    if (parser.isSet(QStringLiteral("microphone-hotkey"))) {
        const auto result = input->bind_microphone(parser.value(QStringLiteral("microphone-hotkey")));
        if (result.status != manager::application::shortcuts::BindingStatus::Active) {
            qWarning() << "Microphone hotkey inactive:" << QString::fromStdString(result.detail);
        }
    }
    return input;
}

int run_session(QApplication &app, manager::presentation::Preferences &preferences, const QString &directory,
                const QCommandLineParser &parser) {
    const manager::adapters::NativeDiagnostic diagnostic;
    const manager::application::ReadRuntimeStatus read(diagnostic);
    manager::presentation::DiagnosticOsd osd(preferences);
    manager::session::UpdateScheduler updates(preferences, directory);
    manager::adapters::SessionServer server(read, osd);
    const auto input = session_input(preferences, parser);
    if (!server.listen()) {
        return 1;
    }
    manager::session::DeviceStatePublisher state;
    manager::session::VendorActions vendor_input(
        preferences,
        [] {
            manager::adapters::launch_component(QStringLiteral("redmibook-manager"),
                                                {QStringLiteral("--overview")});
        },
        [&state] { state.refresh(); });
    QObject::connect(&server, &manager::adapters::SessionServer::openTray, &app,
                     [] { manager::adapters::launch_component(QStringLiteral("redmibook-tray")); });
    QObject::connect(&server, &manager::adapters::SessionServer::restartRequested, &app,
                     [] { manager::adapters::launch_component(QStringLiteral("redmibook-session")); });
    if (parser.isSet(QStringLiteral("with-tray")) && preferences.tray_autostart()) {
        manager::adapters::launch_component(QStringLiteral("redmibook-tray"));
    }
    return app.exec();
}

void configure(QCommandLineParser &parser) {
    parser.addHelpOption();
    parser.addOption({QStringLiteral("restored"), QStringLiteral("Restored by installer lifecycle")});
    parser.addOption({QStringLiteral("with-tray"), QStringLiteral("Open tray client")});
    parser.addOption(
        {QStringLiteral("endpoint"), QStringLiteral("Print this session IPC endpoint and exit")});
    parser.addOption({QStringLiteral("probe"), QStringLiteral("Send bounded diagnostic request")});
    parser.addOption({QStringLiteral("shutdown"), QStringLiteral("Stop session background runtime")});
    parser.addOption({QStringLiteral("microphone-hotkey"),
                      QStringLiteral("Optional session microphone shortcut, e.g. Ctrl+Alt+M"),
                      QStringLiteral("chord")});
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
    if (parser.isSet(QStringLiteral("endpoint"))) {
        QTextStream(stdout) << manager::adapters::session_endpoint() << Qt::endl;
        return 0;
    }
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
    return run_session(app, preferences, store.directory(), parser);
}
