#include "apps/desktop/smoke_test.hpp"

#include <QtCore/QCoreApplication>
#include <QtCore/QTimer>
#include <QtQml/QQmlApplicationEngine>
#include <QtQuick/QQuickItem>
#include <QtQuick/QQuickWindow>

namespace manager::desktop {
namespace {
bool check_window(const QQmlApplicationEngine &engine, const QString &screenshot) {
    if (engine.rootObjects().size() != 1) {
        return false;
    }
    auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
    if (window == nullptr || !window->isVisible()) {
        return false;
    }
    if (window->title() != QStringLiteral("RedmiBook Manager")) {
        return false;
    }
    return screenshot.isEmpty() || window->grabWindow().save(screenshot);
}
} // namespace

void schedule_smoke_test(QQmlApplicationEngine &engine, const QString &screenshot) {
    QTimer::singleShot(500, &engine, [&engine, screenshot] {
        QCoreApplication::exit(check_window(engine, screenshot) ? 0 : 1);
    });
}
} // namespace manager::desktop
