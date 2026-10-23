#include "adapters/integration/preset_executor.hpp"
#include "adapters/local_api/preference_authority.hpp"
#include "adapters/local_api/session_client.hpp"
#include "adapters/local_api/session_server.hpp"
#include "adapters/platform/firmware_controls.hpp"
#include "adapters/platform/power_controls.hpp"
#include "adapters/platform/power_scheme.hpp"
#include "adapters/platform/runtime.hpp"
#include "adapters/platform/session_controls.hpp"
#include "adapters/storage/ui_preferences/store.hpp"
#include "application/runtime/connection_health.hpp"
#include "apps/session/device_state_publisher.hpp"
#include "apps/session/input_actions.hpp"
#include "apps/session/update_scheduler.hpp"
#include "apps/session/vendor_actions.hpp"
#include "presentation/common/preferences.hpp"
#include "presentation/osd/osd_presenter.hpp"
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

std::unique_ptr<manager::session::InputActions> session_input(manager::presentation::OsdPresenter &osd,
                                                              manager::session::WorkQueues &work,
                                                              const QCommandLineParser &parser) {
    auto input = std::make_unique<manager::session::InputActions>(osd, work);
    if (parser.isSet(QStringLiteral("microphone-hotkey"))) {
        const auto result = input->bind_microphone(parser.value(QStringLiteral("microphone-hotkey")));
        if (result.status != manager::application::shortcuts::BindingStatus::Active) {
            qWarning() << "Microphone hotkey inactive:" << QString::fromStdString(result.detail);
        }
    }
    return input;
}

class SessionRuntime final {
  public:
    SessionRuntime(manager::presentation::Preferences &preferences, const QString &directory,
                   const QCommandLineParser &parser)
        : read_(diagnostic_), osd_(preferences), updates_(preferences, directory),
          server_(read_, osd_, &osd_), input_(session_input(osd_, work_, parser)), state_(work_),
          vendor_(
              preferences, osd_, work_,
              [] {
                  manager::adapters::launch_component(QStringLiteral("redmibook-manager"),
                                                      {QStringLiteral("--overview")});
              },
              [] {
                  manager::adapters::launch_component(QStringLiteral("redmibook-tray"),
                                                      {QStringLiteral("--open-dashboard")});
              },
              [this] { state_.refresh(); }) {
        state_.setMicrophoneReconciliation([this] { vendor_.reconcileMicrophone(); });
        ready = server_.listen();
        QObject::connect(&server_, &manager::adapters::SessionServer::openTray, &server_,
                         [] { manager::adapters::launch_component(QStringLiteral("redmibook-tray")); });
        QObject::connect(&server_, &manager::adapters::SessionServer::restartRequested, &server_,
                         [] { manager::adapters::launch_component(QStringLiteral("redmibook-session")); });
        if (ready && parser.isSet(QStringLiteral("with-tray")) && preferences.tray_autostart())
            manager::adapters::launch_component(QStringLiteral("redmibook-tray"));
    }
    bool ready{false};

  private:
    manager::session::WorkQueues work_;
    manager::adapters::NativeDiagnostic diagnostic_;
    manager::application::ReadRuntimeStatus read_;
    manager::presentation::OsdPresenter osd_;
    manager::session::UpdateScheduler updates_;
    manager::adapters::SessionServer server_;
    std::unique_ptr<manager::session::InputActions> input_;
    manager::session::DeviceStatePublisher state_;
    manager::session::VendorActions vendor_;
};
int run_session(QApplication &app, manager::presentation::Preferences &preferences, const QString &directory,
                const QCommandLineParser &parser, manager::adapters::PreferenceClient &client) {
    std::unique_ptr<SessionRuntime> runtime;
    QObject boot_context;
    const auto start = [&] {
        if (runtime || !client.read() || !preferences.available())
            return;
        runtime = std::make_unique<SessionRuntime>(preferences, directory, parser);
        if (!runtime->ready)
            app.exit(1);
    };
    QObject::connect(&preferences, &manager::presentation::Preferences::changed, &boot_context, start);
    QTimer::singleShot(0, &boot_context, start);
    return app.exec();
}

void configure(QCommandLineParser &parser) {
    parser.addHelpOption();
    parser.addOption({QStringLiteral("restored"), QStringLiteral("Restored by installer lifecycle")});
    parser.addOption({QStringLiteral("with-tray"), QStringLiteral("Open tray client")});
    parser.addOption(
        {QStringLiteral("endpoint"), QStringLiteral("Print this session IPC endpoint and exit")});
    parser.addOption({QStringLiteral("preferences-endpoint"),
                      QStringLiteral("Print per-user preferences IPC endpoint and exit")});
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
    if (parser.isSet(QStringLiteral("preferences-endpoint"))) {
        QTextStream(stdout) << manager::adapters::preferences_endpoint() << Qt::endl;
        return 0;
    }
    if (parser.isSet(QStringLiteral("endpoint"))) {
        QTextStream(stdout) << manager::adapters::session_endpoint() << Qt::endl;
        return 0;
    }
    if (parser.isSet(QStringLiteral("probe")) || parser.isSet(QStringLiteral("shutdown"))) {
        return request_status(app, parser);
    }
    const auto directory = manager::adapters::preferences_directory();
    QDir().mkpath(directory);
    QLockFile lock(QDir(directory).filePath(manager::adapters::session_endpoint() + QStringLiteral(".lock")));
    if (!lock.tryLock(2000))
        return 0;
    manager::adapters::PreferenceAuthority authority(
        directory, manager::adapters::preferences_endpoint(), [] {
            using namespace manager::adapters;
            PresetExecutor executor(make_core_performance_control(), std::make_shared<NativeRefresh>(),
                                    make_power_schemes(), make_brightness(), make_energy_saver());
            return executor.choices();
        });
    manager::presentation::Preferences preferences(authority.client());
    return run_session(app, preferences, directory, parser, authority.client());
}
