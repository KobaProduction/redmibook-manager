#include "apps/desktop/popup_smoke.hpp"
#include <QCoreApplication>
#include <QDebug>
#include <QPointer>
#include <QQuickWindow>
#include <QTimer>
namespace manager::desktop {
namespace {
bool popup_is_light(QObject &popup, const QString &path) {
    auto *background = popup.property("background").value<QQuickItem *>();
    if (!popup.property("visible").toBool() || background == nullptr || background->window() == nullptr) {
        return false;
    }
    auto *window = background->window();
    const auto image = window->grabWindow();
    if (image.isNull()) {
        return false;
    }
    const auto point = background->mapToScene(QPointF(background->width() - 12, background->height() - 12));
    const auto scale = double(image.width()) / window->width();
    const auto color =
        image.pixelColor(static_cast<int>(point.x() * scale), static_cast<int>(point.y() * scale));
    const bool saved = path.isEmpty() || image.save(path + QStringLiteral(".popup.png"));
    qInfo() << "Popup render" << color << point << background->size() << window->size();
    return color.lightness() >= 128 && saved;
}
} // namespace
void verify_language_popup(QQuickItem &selector, const QString &path) {
    const QPointer<QObject> popup(selector.property("popup").value<QObject *>());
    if (!popup || !QMetaObject::invokeMethod(popup, "open")) {
        QCoreApplication::exit(1);
        return;
    }
    QTimer::singleShot(250, &selector, [popup, path] {
        const bool valid = popup && popup_is_light(*popup, path);
        if (popup) {
            QMetaObject::invokeMethod(popup, "close");
        }
        QCoreApplication::exit(valid ? 0 : 1);
    });
}
} // namespace manager::desktop
