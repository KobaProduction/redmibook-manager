#include "adapters/storage/ui_preferences/store.hpp"
#include "presentation/osd/diagnostic_osd.hpp"
#include "presentation/osd/placement.hpp"
#include "presentation/tray/dashboard.hpp"
#include <QApplication>
#include <QScreen>
#include <QTemporaryDir>

namespace {
class SessionFake final : public manager::application::SessionPort {
  public:
    int requests{0};
    bool request(manager::application::SessionCommand,
                 std::function<void(manager::application::SessionReply)> completion) override {
        ++requests;
        completion({});
        return true;
    }
};
bool dashboard_contract() {
    SessionFake session;
    manager::presentation::RuntimeMonitor monitor(session);
    manager::presentation::TrayDashboard dashboard(monitor);
    dashboard.setDeviceState({.profileIndex = 0, .profileAvailable = true, .status = {}});
    int requested = -1;
    QObject::connect(&dashboard, &manager::presentation::TrayDashboard::profileRequested,
                     [&requested](int index) { requested = index; });
    dashboard.toggle(QRect(300, 300, 24, 24));
    auto *selector = dashboard.findChild<QToolButton *>(QStringLiteral("performanceMode2"));
    if (!dashboard.isVisible() || selector == nullptr || !selector->isEnabled()) {
        return false;
    }
    selector->click();
    return session.requests == 1 && requested == 2 && !selector->isChecked();
}
bool osd_contract() {
    QTemporaryDir directory;
    manager::adapters::UiPreferenceStore store(directory.path());
    manager::presentation::Preferences preferences(store);
    manager::presentation::DiagnosticOsd osd(preferences);
    osd.show_result({});
    const auto expected = QGuiApplication::primaryScreen()->availableGeometry().topLeft() + QPoint(24, 24);
    if (osd.pos() != expected) {
        return false;
    }
    const QRect area(100, 200, 1000, 800);
    const QSize size(200, 100);
    const std::array<QPoint, 9> positions{QPoint(124, 224), QPoint(500, 224), QPoint(876, 224),
                                          QPoint(124, 550), QPoint(500, 550), QPoint(876, 550),
                                          QPoint(124, 876), QPoint(500, 876), QPoint(876, 876)};
    for (std::size_t index = 0; index < positions.size(); ++index) {
        if (manager::presentation::feedback_position(
                area, size, manager::application::osd_positions[index]) != positions[index]) {
            return false;
        }
    }
    return true;
}
} // namespace

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    return dashboard_contract() && osd_contract() ? 0 : 1;
}
