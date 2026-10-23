#include "adapters/storage/ui_preferences/store.hpp"
#include "presentation/osd/osd_presenter.hpp"
#include "presentation/osd/placement.hpp"
#include "presentation/tray/dashboard.hpp"
#include "presentation/tray/tray_client.hpp"
#include <QApplication>
#include <QScreen>
#include <QScrollBar>
#include <QTemporaryDir>
#include <algorithm>
#include <cstdio>

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
bool unsupported_controls_contract() {
    SessionFake session;
    manager::presentation::RuntimeMonitor monitor(session);
    manager::presentation::TrayDashboard dashboard(monitor);
    dashboard.setDeviceState({});
    dashboard.toggle(QRect(300, 300, 24, 24));
    auto *mode = dashboard.findChild<QToolButton *>(QStringLiteral("performanceMode0"));
    if (!mode || mode->isVisibleTo(&dashboard))
        return false;
    manager::presentation::TrayDeviceState failed;
    failed.profileSupported = true;
    dashboard.setDeviceState(failed);
    return mode->isVisibleTo(&dashboard) && !mode->isEnabled();
}
bool rapid_click_contract() {
    class PendingSession final : public manager::application::SessionPort {
      public:
        int requests{0};
        std::function<void(manager::application::SessionReply)> pending;
        bool request(manager::application::SessionCommand,
                     std::function<void(manager::application::SessionReply)> completion) override {
            ++requests;
            pending = std::move(completion);
            return true;
        }
    } source;
    QTemporaryDir directory;
    manager::adapters::UiPreferenceStore store(directory.path());
    manager::presentation::Preferences preferences(store);
    manager::presentation::TrayClient tray(preferences, source);
    int managers = 0;
    QObject::connect(&tray, &manager::presentation::TrayClient::openManager, [&] { ++managers; });
    for (int index = 0; index < 32; ++index) {
        tray.activate(QSystemTrayIcon::Trigger);
        tray.activate(QSystemTrayIcon::DoubleClick);
    }
    return managers == 0 && source.requests == 1;
}
manager::presentation::TrayDeviceState long_presets_fixture() {
    manager::presentation::TrayDeviceState state;
    for (int row = 0; row < 16; ++row) {
        manager::application::presets::Preset preset{std::to_string(row), "Preset", true, {}};
        for (int column = 0; column < 8; ++column)
            preset.buttons.push_back(
                {"button-" + std::to_string(row) + "-" + std::to_string(column), "Action", "quiet", {}});
        state.presets.push_back(std::move(preset));
    }
    return state;
}
bool long_presets_contract() {
    SessionFake session;
    manager::presentation::RuntimeMonitor monitor(session);
    manager::presentation::TrayDashboard dashboard(monitor);
    auto state = long_presets_fixture();
    dashboard.setDeviceState(std::move(state));
    dashboard.toggle(QRect(300, 300, 24, 24));
    auto *scroll = dashboard.findChild<QScrollArea *>(QStringLiteral("presetScroll"));
    auto *first = dashboard.findChild<QPushButton *>(QStringLiteral("button-0-0"));
    auto *last = dashboard.findChild<QPushButton *>(QStringLiteral("button-15-7"));
    if (!scroll || !first || !last || dashboard.height() > dashboard.screen()->availableGeometry().height()) {
        std::fprintf(stderr, "Preset geometry: window=%d minimum=%d hint=%d screen=%d scroll=%d\n",
                     dashboard.height(), dashboard.minimumHeight(), dashboard.sizeHint().height(),
                     dashboard.screen()->availableGeometry().height(), scroll ? scroll->height() : -1);
        return false;
    }
    scroll->ensureWidgetVisible(first);
    const bool first_visible = scroll->viewport()->rect().intersects(
        QRect(first->mapTo(scroll->viewport(), QPoint{}), first->size()));
    scroll->ensureWidgetVisible(last);
    return first_visible && scroll->verticalScrollBar()->maximum() > 0 &&
           scroll->viewport()->rect().intersects(
               QRect(last->mapTo(scroll->viewport(), QPoint{}), last->size())) &&
           std::ranges::all_of(dashboard.findChildren<QPushButton *>(QString{}, Qt::FindDirectChildrenOnly),
                               [&dashboard](QPushButton *button) {
                                   return !button->isVisibleTo(&dashboard) ||
                                          dashboard.rect().contains(button->geometry());
                               });
}
bool osd_contract() {
    QTemporaryDir directory;
    manager::adapters::UiPreferenceStore store(directory.path());
    manager::presentation::Preferences preferences(store);
    manager::presentation::OsdPresenter osd(preferences);
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
bool checked(const char *name, bool result) {
    if (!result)
        std::fprintf(stderr, "Tray contract failed: %s\n", name);
    return result;
}
} // namespace

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    return checked("unsupported", unsupported_controls_contract()) &&
                   checked("dashboard", dashboard_contract()) &&
                   checked("rapid-click", rapid_click_contract()) &&
                   checked("long-presets", long_presets_contract()) && checked("osd", osd_contract())
               ? 0
               : 1;
}
