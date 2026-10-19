#include "apps/desktop/ui_contract_smoke.hpp"
#include "apps/desktop/popup_smoke.hpp"
#include <QApplication>
#include <QDebug>
#include <QQuickItem>
#include <QQuickWindow>
#include <QTimer>

namespace manager::desktop {
namespace {
QQuickItem *find_item(QQuickItem *item, const QString &name) {
    if (item->objectName() == name) {
        return item;
    }
    for (auto *child : item->childItems()) {
        if (auto *found = find_item(child, name)) {
            return found;
        }
    }
    return nullptr;
}

bool check_language(const QQmlApplicationEngine &engine, const QString &expected) {
    if (engine.rootObjects().size() != 1) {
        return false;
    }
    const auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
    if (window == nullptr) {
        return false;
    }
    const auto *label = find_item(window->contentItem(), QStringLiteral("navigation3"));
    const auto *status = find_item(window->contentItem(), QStringLiteral("updateStatus"));
    const auto expected_status = expected == QStringLiteral("Settings")
                                     ? QStringLiteral("Updates have not been checked yet.")
                                     : QStringLiteral("Обновления ещё не проверялись.");
    qInfo() << "Language contract" << expected;
    return label != nullptr && label->property("text").toString() == expected && status != nullptr &&
           status->property("text").toString() == expected_status;
}

bool save_screenshot(const QQmlApplicationEngine &engine, const QString &path) {
    if (path.isEmpty()) {
        return true;
    }
    auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
    return window != nullptr && window->grabWindow().save(path);
}

bool scene_colors(const QQmlApplicationEngine &engine, bool dark) {
    auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
    auto *control = find_item(window->contentItem(), QStringLiteral("languageSelector"));
    if (control == nullptr) {
        return false;
    }
    const auto image = window->grabWindow();
    if (image.isNull()) {
        return false;
    }
    const auto point = control->mapToScene(QPointF(control->width() * 0.75, control->height() * 0.5));
    const auto scale = double(image.width()) / window->width();
    const auto background = image.pixelColor(image.width() / 2, image.height() - 3);
    const auto selector =
        image.pixelColor(static_cast<int>(point.x() * scale), static_cast<int>(point.y() * scale));
    qInfo() << "Rendered colors" << background << selector << "dark" << dark;
    return (background.lightness() < 128) == dark && (selector.lightness() < 128) == dark;
}

void finish_english(QQmlApplicationEngine &engine, presentation::ManagerModel &model,
                    const QString &path = {}) {
    engine.rootObjects().first()->setProperty("page", 3);
    auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
    auto *scroll = find_item(window->contentItem(), QStringLiteral("settingsScroll"));
    if (auto *content = scroll->property("contentItem").value<QObject *>()) {
        content->setProperty("contentY", 0);
    }
    model.setLanguage(1);
    model.setTheme(1);
    QTimer::singleShot(250, &engine, [&engine, &model, path] {
        const bool valid =
            check_language(engine, QStringLiteral("Settings")) && model.languageIndex() == 1 &&
            scene_colors(engine, false) &&
            save_screenshot(engine, path.isEmpty() ? QString{} : path + QStringLiteral(".light.png"));
        if (!valid) {
            QCoreApplication::exit(1);
            return;
        }
        auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
        verify_language_popup(*find_item(window->contentItem(), QStringLiteral("languageSelector")), path);
    });
}

void preview_license(QQmlApplicationEngine &engine, presentation::ManagerModel &model, const QString &path) {
    auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
    auto *button = find_item(window->contentItem(), QStringLiteral("licensesButton"));
    if (button == nullptr || !QMetaObject::invokeMethod(button, "clicked")) {
        QCoreApplication::exit(1);
        return;
    }
    QTimer::singleShot(250, &engine, [&engine, &model, path] {
        auto *dialog = engine.rootObjects().first()->findChild<QObject *>(QStringLiteral("licenseDialog"));
        if (dialog == nullptr || !dialog->property("visible").toBool() ||
            !save_screenshot(engine, path + QStringLiteral(".license.png"))) {
            QCoreApplication::exit(1);
            return;
        }
        QMetaObject::invokeMethod(dialog, "close");
        finish_english(engine, model, path);
    });
}

void preview_about(QQmlApplicationEngine &engine, presentation::ManagerModel &model, const QString &path) {
    engine.rootObjects().first()->setProperty("page", 5);
    QTimer::singleShot(150, &engine, [&engine, &model, path] {
        if (!save_screenshot(engine, path + QStringLiteral(".about.png"))) {
            QCoreApplication::exit(1);
            return;
        }
        preview_license(engine, model, path);
    });
}
void preview_indicators(QQmlApplicationEngine &engine, presentation::ManagerModel &model,
                        const QString &path) {
    if (path.isEmpty()) {
        finish_english(engine, model);
        return;
    }
    auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
    auto *scroll = find_item(window->contentItem(), QStringLiteral("settingsScroll"));
    auto *content = scroll == nullptr ? nullptr : scroll->property("contentItem").value<QObject *>();
    if (content != nullptr) {
        content->setProperty("contentY", content->property("contentHeight").toReal() -
                                             content->property("height").toReal());
    }
    QTimer::singleShot(150, &engine, [&engine, &model, path] {
        if (!save_screenshot(engine, path + QStringLiteral(".indicator.png"))) {
            QCoreApplication::exit(1);
            return;
        }
        preview_about(engine, model, path);
    });
}
} // namespace

void schedule_ui_contract(QQmlApplicationEngine &engine, presentation::ManagerModel &model,
                          const QString &screenshot) {
    QTimer::singleShot(400, &engine, [&engine, &model, screenshot] {
        engine.rootObjects().first()->setProperty("page", 3);
        model.setLanguage(2);
        model.setTheme(2);
        QTimer::singleShot(150, &engine, [&engine, &model, screenshot] {
            const bool valid = check_language(engine, QStringLiteral("Настройки")) &&
                               model.themeIndex() == 2 && scene_colors(engine, true) &&
                               QApplication::palette().color(QPalette::Window).lightness() < 128 &&
                               save_screenshot(engine, screenshot);
            if (!valid) {
                QCoreApplication::exit(1);
                return;
            }
            preview_indicators(engine, model, screenshot);
        });
    });
}
} // namespace manager::desktop
