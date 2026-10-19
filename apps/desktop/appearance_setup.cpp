#include "apps/desktop/appearance_setup.hpp"
#include "adapters/platform/window_appearance.hpp"
#include <QCoreApplication>
#include <QWindow>
namespace manager::desktop {
namespace {
void round_windows(const QQmlApplicationEngine &engine, bool rounded) {
    for (auto *object : engine.rootObjects()) {
        if (auto *window = qobject_cast<QWindow *>(object)) {
            adapters::set_window_rounding(static_cast<std::uintptr_t>(window->winId()), rounded);
        }
    }
}
} // namespace
void wire_manager_appearance(presentation::Preferences &preferences, presentation::ManagerModel &model,
                             QQmlApplicationEngine &engine) {
    const auto apply = [&engine, &preferences] { round_windows(engine, preferences.rounded_corners()); };
    QObject::connect(&preferences, &presentation::Preferences::changed, &engine, apply);
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreated, &engine, apply);
    QObject::connect(&model, &presentation::ManagerModel::restartRequested, &engine,
                     [] { QCoreApplication::exit(restart_manager_exit); });
}
} // namespace manager::desktop
