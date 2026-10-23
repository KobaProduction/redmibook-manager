#include "adapters/local_api/preference_channel.hpp"
#include "adapters/platform/development_scope.hpp"
#include "adapters/platform/host_info.hpp"
#include "application/overview/read_overview.hpp"
#include "apps/desktop/appearance_setup.hpp"
#include "apps/desktop/loading_preview.hpp"
#include "apps/desktop/manager_host.hpp"
#include "apps/desktop/smoke_test.hpp"
#include "apps/desktop/style_setup.hpp"
#include "presentation/overview_model.hpp"
#include <QApplication>
#include <QMessageBox>
#include <QProcess>
#include <QQuickStyle>
#include <QTimer>

#include <QtCore/QCommandLineParser>
#include <QtGui/QGuiApplication>
#include <QtQml/QQmlApplicationEngine>

namespace {
void configure_parser(QCommandLineParser &parser) {
    parser.setApplicationDescription(QStringLiteral("Native laptop manager foundation"));
    parser.addHelpOption();
    parser.addOption({QStringLiteral("settings"), QStringLiteral("Open Settings")});
    parser.addOption({QStringLiteral("overview"), QStringLiteral("Open Overview")});
    parser.addOption({QStringLiteral("restored"), QStringLiteral("Restored by installer lifecycle")});
    parser.addOption(
        {QStringLiteral("ui-contract"), QStringLiteral("Verify live language and theme changes")});
    parser.addOption(
        {QStringLiteral("loading-preview"), QStringLiteral("Render the isolated loading skeleton")});
    parser.addVersionOption();
    parser.addOption({QStringLiteral("smoke-test"), QStringLiteral("Check the UI and exit")});
    parser.addOption({QStringLiteral("preset-preview"),
                      QStringLiteral("Render isolated tray settings and button editor")});
    parser.addOption({QStringLiteral("screenshot"), QStringLiteral("Save a smoke-test screenshot"),
                      QStringLiteral("path")});
}
} // namespace

namespace {
int run_connected_window(QApplication &app, const QCommandLineParser &parser) {
    const auto activation = parser.isSet(QStringLiteral("overview"))
                                ? manager::desktop::ManagerActivation::Overview
                                : manager::desktop::ManagerActivation::KeepCurrent;
    if (manager::desktop::ManagerInstance::activate_existing(activation))
        return 0;
    auto remote =
        std::make_unique<manager::adapters::PreferenceClient>(manager::adapters::preferences_endpoint());
    manager::desktop::ManagerHost host(false, std::move(remote));
    host.setRestored(parser.isSet(QStringLiteral("restored")));
    const int opened = host.open(false, {}, false, activation, false);
    return opened >= 0 ? opened : app.exec();
}
int run_window(QApplication &app, const QCommandLineParser &parser) {
    const bool ui_contract = parser.isSet(QStringLiteral("ui-contract"));
    const bool preset_preview = parser.isSet(QStringLiteral("preset-preview"));
    const bool loading = parser.isSet(QStringLiteral("loading-preview"));
    const bool smoke = parser.isSet(QStringLiteral("smoke-test")) || ui_contract || preset_preview || loading;
    if (!smoke)
        return run_connected_window(app, parser);
    std::unique_ptr<manager::application::PreferencePort> store;
    if (loading)
        store = std::make_unique<manager::desktop::LoadingPreview>();
    manager::desktop::ManagerHost host(smoke, std::move(store));
    host.setRestored(parser.isSet(QStringLiteral("restored")));
    const auto activation = parser.isSet(QStringLiteral("overview"))
                                ? manager::desktop::ManagerActivation::Overview
                                : manager::desktop::ManagerActivation::KeepCurrent;
    const int opened =
        host.open(smoke, parser.value(QStringLiteral("screenshot")), ui_contract, activation, preset_preview);
    return opened >= 0 ? opened : app.exec();
}
} // namespace
int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    manager::adapters::configure_gui_identity();
    app.setStyle(QStringLiteral("Fusion"));
    QCommandLineParser parser;
    configure_parser(parser);
    parser.process(app);
    const int result = run_window(app, parser);
    if (result == manager::desktop::restart_manager_exit) {
        if (!manager::adapters::development_scope().empty()) {
            return 0;
        }
        if (QProcess::startDetached(QCoreApplication::applicationFilePath(),
                                    {QStringLiteral("--settings")})) {
            return 0;
        }
        QMessageBox::critical(nullptr, QObject::tr("Restart failed"),
                              QObject::tr("Could not restart Manager. Open it from the Start menu."));
        return 1;
    }
    return result;
}
