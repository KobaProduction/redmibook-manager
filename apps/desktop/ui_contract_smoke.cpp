#include "apps/desktop/ui_contract_smoke.hpp"
#include "apps/desktop/popup_smoke.hpp"
#include "apps/desktop/preview_presets.hpp"
#include "presentation/device_controls_model.hpp"
#include "presentation/presets/presets_model.hpp"
#include <QApplication>
#include <QDebug>
#include <QMouseEvent>
#include <QQuickItem>
#include <QQuickWindow>
#include <QTimer>
#include <array>

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
    const auto *label = find_item(window->contentItem(), QStringLiteral("navigation4"));
    const auto *status = find_item(window->contentItem(), QStringLiteral("updateStatus"));
    const auto expected_status = expected == QStringLiteral("Settings")
                                     ? QStringLiteral("Updates: Not checked yet.")
                                     : QStringLiteral("Обновления: Ещё не проверялись.");
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
    engine.rootObjects().first()->setProperty("page", 4);
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

bool battery_lease_lifecycle(QQmlApplicationEngine &engine, presentation::ManagerModel &model,
                             const std::vector<int> &requests) {
    if (requests.empty() || requests.back() != 1000)
        return false;
    model.setBatteryIntervalMs(100);
    if (requests.back() != 100)
        return false;
    engine.rootObjects().first()->setProperty("page", 4);
    const bool released = requests.back() == 0;
    model.setBatteryIntervalMs(1000);
    return released;
}
void capture_icon_choices(QQmlApplicationEngine &engine, presentation::ManagerModel &model,
                          const QString &path, bool captureOnly, QObject *editor) {
    auto *picker = engine.rootObjects().first()->findChild<QObject *>(QStringLiteral("presetIconPicker"));
    if (!picker || !QMetaObject::invokeMethod(picker, "open")) {
        QCoreApplication::exit(1);
        return;
    }
    QTimer::singleShot(150, &engine, [&engine, &model, path, captureOnly, editor, picker] {
        const bool captured = picker->property("visible").toBool() &&
                              save_screenshot(engine, path + QStringLiteral(".preset-icons.png"));
        QMetaObject::invokeMethod(picker, "close");
        QMetaObject::invokeMethod(editor, "close");
        if (!captured || captureOnly)
            QCoreApplication::exit(captured ? 0 : 1);
        else
            finish_english(engine, model, path);
    });
}

bool populate_preview_buttons(presentation::PresetsModel &presets, const QString &id,
                              const QVariantList &buttons) {
    const auto demo = preview::demo_presets().front();
    presets.renamePreset(id, QString::fromStdString(demo.title));
    for (qsizetype index = 0; index < buttons.size(); ++index) {
        const auto &button = demo.buttons[static_cast<std::size_t>(index)];
        QVariantList actions;
        for (const auto &action : button.actions)
            actions.append(QVariantMap{{"kind", static_cast<int>(action.kind)},
                                       {"value", QString::fromStdString(action.value)}});
        if (!presets.saveButton(id, {{"id", buttons[index].toMap().value("id")},
                                     {"title", QString::fromStdString(button.title)},
                                     {"icon", QString::fromStdString(button.icon)},
                                     {"actions", actions}}))
            return false;
    }
    return true;
}
void capture_preview_editor(QQmlApplicationEngine &engine, presentation::ManagerModel &model,
                            const QString &path, bool captureOnly) {
    auto *editor = engine.rootObjects().first()->findChild<QObject *>(QStringLiteral("presetButtonEditor"));
    const auto *presets =
        engine.rootObjects().first()->property("presets").value<presentation::PresetsModel *>();
    const auto preset = presets->items().first().toMap();
    const auto button = preset.value("buttons").toList().first();
    if (!editor || !QMetaObject::invokeMethod(editor, "edit", Q_ARG(QVariant, QVariant(preset)),
                                              Q_ARG(QVariant, button))) {
        QCoreApplication::exit(1);
        return;
    }
    QTimer::singleShot(300, &engine, [&engine, &model, editor, path, captureOnly] {
        if (!editor->property("visible").toBool() ||
            !save_screenshot(engine, path + QStringLiteral(".preset-editor.png"))) {
            QCoreApplication::exit(1);
            return;
        }
        capture_icon_choices(engine, model, path, captureOnly, editor);
    });
}
bool ensure_preview_presets(presentation::PresetsModel &presets) {
    if (!presets.items().empty())
        return true;
    presets.addPreset();
    const auto preset = presets.items().first().toMap();
    return populate_preview_buttons(presets, preset.value("id").toString(), preset.value("buttons").toList());
}
void preview_presets(QQmlApplicationEngine &engine, presentation::ManagerModel &model, const QString &path,
                     bool captureOnly = false) {
    auto *root = engine.rootObjects().first();
    auto *presets = root->property("presets").value<presentation::PresetsModel *>();
    if (!presets || !ensure_preview_presets(*presets)) {
        QCoreApplication::exit(1);
        return;
    }
    root->setProperty("page", 6);
    QTimer::singleShot(150, &engine, [&engine, &model, path, captureOnly] {
        if (!save_screenshot(engine, path + QStringLiteral(".tray-settings.png"))) {
            QCoreApplication::exit(1);
            return;
        }
        capture_preview_editor(engine, model, path, captureOnly);
    });
}
void preview_compact_pages(QQmlApplicationEngine &engine, presentation::ManagerModel &model,
                           const QString &path, int step = 0) {
    if (step == 4) {
        preview_presets(engine, model, path, true);
        return;
    }
    auto *root = engine.rootObjects().first();
    const std::array pages{0, 4, 2, 5};
    root->setProperty("page", pages[static_cast<std::size_t>(step)]);
    root->setProperty("height", step == 1 ? 1800 : 1280);
    QTimer::singleShot(150, &engine, [&engine, &model, path, step] {
        const std::array suffixes{".overview.png", ".settings.png", ".hotkeys.png", ".about.png"};
        if (!save_screenshot(engine, path + QLatin1String(suffixes[static_cast<std::size_t>(step)]))) {
            QCoreApplication::exit(1);
            return;
        }
        preview_compact_pages(engine, model, path, step + 1);
    });
}

void preview_device_controls(QQmlApplicationEngine &engine, presentation::ManagerModel &model,
                             const QString &path, bool captureOnly) {
    auto *root = engine.rootObjects().first();
    root->setProperty("page", 3);
    if (captureOnly && QGuiApplication::platformName() == QStringLiteral("offscreen"))
        root->setProperty("height", 1280);
    QTimer::singleShot(150, &engine, [&engine, &model, path, captureOnly] {
        if (!save_screenshot(engine, path + QStringLiteral(".device-controls.png"))) {
            QCoreApplication::exit(1);
            return;
        }
        if (captureOnly)
            preview_compact_pages(engine, model, path);
        else
            preview_presets(engine, model, path, false);
    });
}

application::overview::DeviceState battery_fixture() {
    application::overview::DeviceState fixture;
    fixture.microphone = {application::microphone::Status::Ready, false, "preview"};
    fixture.displays = {application::display::Status::Ready,
                        {{"internal", "Built-in display", 2560, 1600, 120, {60, 120, 165}, true}},
                        "preview"};
    fixture.battery = {
        application::battery::Status::Available, application::battery::ChargeLimit::Protection80, {}};
    fixture.battery_capability = {
        application::battery::Status::Available,
        {application::battery::ChargeLimit::Protection80, application::battery::ChargeLimit::Full},
        {}};
    fixture.battery_telemetry = {application::battery::TelemetryStatus::Available,
                                 true,
                                 true,
                                 false,
                                 79,
                                 65.0,
                                 83.0,
                                 99.0,
                                 16.5,
                                 20.0};
    return fixture;
}
std::shared_ptr<std::vector<int>> observe_battery_requests(presentation::DeviceControlsModel &controls,
                                                           QQmlApplicationEngine &engine) {
    const auto requests = std::make_shared<std::vector<int>>();
    QObject::connect(&controls, &presentation::DeviceControlsModel::batteryWatchRequested, &engine,
                     [requests](int interval) { requests->push_back(interval); });
    return requests;
}
void preview_battery(QQmlApplicationEngine &engine, presentation::ManagerModel &model, const QString &path,
                     bool captureOnly = false) {
    const auto fixture = battery_fixture();
    auto *root = engine.rootObjects().first();
    auto *controls = root->property("controls").value<presentation::DeviceControlsModel *>();
    if (!controls) {
        QCoreApplication::exit(1);
        return;
    }
    controls->acceptState(fixture);
    const auto requests = observe_battery_requests(*controls, engine);
    if (auto *window = qobject_cast<QQuickWindow *>(root);
        window && QGuiApplication::platformName() == QStringLiteral("offscreen"))
        window->requestActivate();
    root->setProperty("page", 3);
    QTimer::singleShot(150, &engine, [&engine, &model, path, requests, captureOnly] {
        if (!save_screenshot(engine, path + QStringLiteral(".battery.png")) ||
            !battery_lease_lifecycle(engine, model, *requests)) {
            QCoreApplication::exit(1);
            return;
        }
        preview_device_controls(engine, model, path, captureOnly);
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
        preview_battery(engine, model, path);
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
void preview_fixed_shortcut(QQmlApplicationEngine &engine, presentation::ManagerModel &model,
                            const QString &path) {
    auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
    auto *binding = find_item(window->contentItem(), QStringLiteral("fnKFixedBinding"));
    if (binding == nullptr) {
        QCoreApplication::exit(1);
        return;
    }
    const auto position = binding->mapToScene(QPointF(binding->width() / 2, binding->height() / 2));
    QMouseEvent hover(QEvent::MouseMove, position, window->mapToGlobal(position.toPoint()), Qt::NoButton,
                      Qt::NoButton, Qt::NoModifier);
    // Synthetic event stays inside this isolated QQuickWindow; never move the desktop pointer.
    QCoreApplication::sendEvent(window, &hover);
    QTimer::singleShot(250, &engine, [&engine, &model, path] {
        save_screenshot(engine, path.isEmpty() ? QString{} : path + QStringLiteral(".hotkeys-tooltip.png"));
        engine.rootObjects().first()->setProperty("page", 4);
        preview_indicators(engine, model, path);
    });
}

void preview_hotkeys(QQmlApplicationEngine &engine, presentation::ManagerModel &model, const QString &path) {
    auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
    auto *button = find_item(window->contentItem(), QStringLiteral("navigation2"));
    if (button == nullptr || !QMetaObject::invokeMethod(button, "clicked") ||
        engine.rootObjects().first()->property("page").toInt() != 2) {
        QCoreApplication::exit(1);
        return;
    }
    QTimer::singleShot(150, &engine, [&engine, &model, path] {
        auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
        auto *selector = find_item(window->contentItem(), QStringLiteral("vendorKeyActionSelector"));
        if (selector == nullptr || !QMetaObject::invokeMethod(selector, "activated", Q_ARG(int, 2)) ||
            model.vendorKeyAction() != 2 ||
            !QMetaObject::invokeMethod(selector, "activated", Q_ARG(int, 1)) ||
            model.vendorKeyAction() != 3) {
            QCoreApplication::exit(1);
            return;
        }
        model.setVendorKeyAction(1);
        if (!save_screenshot(engine, path + QStringLiteral(".hotkeys.png"))) {
            QCoreApplication::exit(1);
            return;
        }
        preview_fixed_shortcut(engine, model, path);
    });
}

} // namespace

void schedule_ui_contract(QQmlApplicationEngine &engine, presentation::ManagerModel &model,
                          const QString &screenshot) {
    QTimer::singleShot(400, &engine, [&engine, &model, screenshot] {
        engine.rootObjects().first()->setProperty("page", 4);
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
            preview_hotkeys(engine, model, screenshot);
        });
    });
}
void schedule_preset_preview(QQmlApplicationEngine &engine, presentation::ManagerModel &model,
                             const QString &screenshot) {
    model.setLanguage(2);
    model.setTheme(qEnvironmentVariable("MANAGER_PREVIEW_THEME") == QStringLiteral("light") ? 1 : 2);
    QTimer::singleShot(150, &engine,
                       [&engine, &model, screenshot] { preview_battery(engine, model, screenshot, true); });
}
} // namespace manager::desktop
