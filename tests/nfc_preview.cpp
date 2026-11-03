#include "application/nfc/ndef.hpp"
#include "presentation/common/appearance/theme.hpp"
#include "presentation/nfc_model.hpp"
#include <QApplication>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickItemGrabResult>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QTimer>
#include <QTranslator>
#include <iostream>
#include <utility>

namespace {
void configure_appearance(QApplication &app, QTranslator &translator,
                          manager::presentation::ThemeController &theme) {
    if (app.arguments().contains(QStringLiteral("ru")) &&
        translator.load(QStringLiteral(":/translations/manager_ru.qm")))
        app.installTranslator(&translator);
    theme.set_mode(app.arguments().contains(QStringLiteral("--dark")) ? QStringLiteral("dark")
                                                                      : QStringLiteral("light"));
}
manager::application::nfc::Snapshot fixture() {
    manager::application::nfc::Snapshot state;
    state.status = manager::application::nfc::Status::Ready;
    state.present = true;
    state.writable = true;
    state.event_driven = true;
    state.name = "NXP NTAG5 — preview";
    state.capacity = 1024;
    state.encounters = 3;
    state.field = false;
    state.content = manager::application::nfc::make_uri_record("https://example.org/");
    return state;
}
void capture(QQuickItem &item, const QString &path) {
    const auto grab = item.grabToImage();
    QObject::connect(grab.get(), &QQuickItemGrabResult::ready, qApp,
                     [grab, path] { QCoreApplication::exit(grab->image().save(path) ? 0 : 4); });
}
std::unique_ptr<QObject> create_card(QQmlEngine &engine, manager::presentation::NfcModel &model) {
    QQmlComponent component(&engine);
    component.setData("import Redmibook.Manager; NfcCard { palette: AppPalette {} }", QUrl());
    std::unique_ptr<QObject> object(
        component.createWithInitialProperties({{"nfc", QVariant::fromValue(&model)}}));
    if (!object)
        std::cerr << component.errorString().toStdString();
    return object;
}
void attach(QQuickWindow &window, QQuickItem &item, const QString &path) {
    item.setParentItem(window.contentItem());
    item.setWidth(740);
    item.setHeight(item.implicitHeight());
    window.resize(740, static_cast<int>(item.height()));
    QObject::connect(&window, &QQuickWindow::frameSwapped, qApp, [&, path, armed = false]() mutable {
        if (!std::exchange(armed, true))
            QMetaObject::invokeMethod(qApp, [&, path] { capture(item, path); });
    });
    QTimer::singleShot(5000, qApp, [] { QCoreApplication::exit(3); });
}
int run_preview(QApplication &app, const QString &path, QTranslator &translator,
                manager::presentation::ThemeController &theme) {
    manager::presentation::NfcModel model({});
    model.acceptState(fixture());
    QQmlEngine engine;
    const auto object = create_card(engine, model);
    auto *item = qobject_cast<QQuickItem *>(object.get());
    if (!item)
        return 2;
    configure_appearance(app, translator, theme);
    engine.retranslate();
    QQuickWindow window;
    attach(window, *item, path);
    window.show();
    return app.exec();
}
} // namespace
int main(int argc, char **argv) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QQuickStyle::setStyle(QStringLiteral("Fusion"));
    QApplication app(argc, argv);
    app.setStyle(QStringLiteral("Fusion"));
    if (argc < 2)
        return 1;
    QTranslator translator;
    manager::presentation::ThemeController theme;
    return run_preview(app, QString::fromLocal8Bit(argv[1]), translator, theme);
}
