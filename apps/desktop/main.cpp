#include "adapters/platform/host_info.hpp"
#include "application/overview/read_overview.hpp"
#include "apps/desktop/appearance_setup.hpp"
#include "apps/desktop/manager_host.hpp"
#include "apps/desktop/smoke_test.hpp"
#include "apps/desktop/style_setup.hpp"
#include "presentation/overview_model.hpp"
#include <QApplication>
#include <QMessageBox>
#include <QProcess>
#include <QQuickStyle>

#include <QtCore/QCommandLineParser>
#include <QtGui/QGuiApplication>
#include <QtQml/QQmlApplicationEngine>

namespace {
void configure_parser(QCommandLineParser &parser) {
    parser.setApplicationDescription(QStringLiteral("Native laptop manager foundation"));
    parser.addHelpOption();
    parser.addOption({QStringLiteral("settings"), QStringLiteral("Open Settings")});
    parser.addOption({QStringLiteral("restored"), QStringLiteral("Restored by installer lifecycle")});
    parser.addOption(
        {QStringLiteral("ui-contract"), QStringLiteral("Verify live language and theme changes")});
    parser.addVersionOption();
    parser.addOption({QStringLiteral("smoke-test"), QStringLiteral("Check the UI and exit")});
    parser.addOption({QStringLiteral("screenshot"), QStringLiteral("Save a smoke-test screenshot"),
                      QStringLiteral("path")});
}
} // namespace

namespace {
int run_window(QApplication &app, const QCommandLineParser &parser) {
    const bool ui_contract = parser.isSet(QStringLiteral("ui-contract"));
    const bool smoke = parser.isSet(QStringLiteral("smoke-test")) || ui_contract;
    manager::desktop::ManagerHost host(smoke);
    host.setRestored(parser.isSet(QStringLiteral("restored")));
    const int opened = host.open(smoke, parser.value(QStringLiteral("screenshot")), ui_contract);
    return opened >= 0 ? opened : app.exec();
}
} // namespace
int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    manager::adapters::configure_gui_identity();
    app.setStyle(QStringLiteral("Fusion"));
    manager::desktop::configure_controls();
    QCommandLineParser parser;
    configure_parser(parser);
    parser.process(app);
    const int result = run_window(app, parser);
    if (result == manager::desktop::restart_manager_exit) {
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
