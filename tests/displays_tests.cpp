#include "adapters/storage/ui_preferences/store.hpp"
#include "presentation/common/displays.hpp"
#include "presentation/osd/osd_presenter.hpp"
#include <QApplication>
#include <QTemporaryDir>
#include <QTextStream>
#include <QWindow>
namespace {
bool geometry_contract() {
    const QRect area(-1920, -200, 1920, 1080);
    const auto fitted = manager::presentation::centered_window(area, QSize(3000, 2000));
    const auto centered = manager::presentation::centered_window(area, QSize(800, 600));
    return fitted == area && area.contains(centered) && centered.center() == area.center();
}
bool check_activation(QWindow &window, QScreen &screen) {
    int activations = 0;
    bool exposed = true;
    const auto focus = [&] {
        ++activations;
        exposed = exposed && window.isExposed();
    };
    manager::presentation::present_window(window, screen.geometry().center(), focus);
    manager::presentation::present_window(window, screen.geometry().center(), focus);
    const bool deferred = activations == 0;
    QCoreApplication::processEvents();
    const bool first = deferred && activations == 1 && exposed;
    manager::presentation::present_window(window, screen.geometry().center(), focus);
    QCoreApplication::processEvents();
    const bool repeated = activations == 2 && exposed;
    return first && repeated;
}
bool check_display(manager::presentation::Preferences &preferences, QScreen &screen) {
    const auto id = manager::presentation::display_id(screen);
    if (manager::presentation::resolve_display(id, {}) != &screen || !preferences.set_osd_screen(id)) {
        return false;
    }
    manager::presentation::OsdPresenter osd(preferences);
    QWindow window;
    window.resize(500, 300);
    osd.show_result({});
    const bool activation = check_activation(window, screen);
    const bool passed = activation && osd.pos() == screen.availableGeometry().topLeft() + QPoint(24, 24) &&
                        window.screen() == &screen && screen.availableGeometry().contains(window.geometry());
    QTextStream(stdout) << "Display: " << screen.name() << " verified: " << passed << Qt::endl;
    return passed;
}

bool displays_contract() {
    QTemporaryDir directory;
    manager::adapters::UiPreferenceStore store(directory.path());
    if (!store.update("language", "en")) {
        return false;
    }
    manager::presentation::Preferences preferences(store);
    for (auto *screen : QGuiApplication::screens()) {
        if (!check_display(preferences, *screen)) {
            return false;
        }
    }
    return !QGuiApplication::screens().isEmpty();
}
} // namespace
int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    return geometry_contract() && displays_contract() ? 0 : 1;
}
