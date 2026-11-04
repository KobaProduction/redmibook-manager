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
#include <algorithm>
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
manager::application::nfc::Bytes service_record() {
    const std::string type = "com.xiaomi.mi_connect_service:externaltype";
    manager::application::nfc::Bytes bytes{0xd4, static_cast<std::uint8_t>(type.size()), 3};
    bytes.insert(bytes.end(), type.begin(), type.end());
    bytes.insert(bytes.end(), {0, 1, 0xa5});
    return bytes;
}
manager::application::nfc::Snapshot fixture() {
    manager::application::nfc::Snapshot state;
    state.status = manager::application::nfc::Status::Ready;
    state.present = true;
    state.writable = true;
    state.event_driven = true;
    state.name = "NXP NTAG5";
    state.capacity = 1024;
    state.max_content_size = 1020;
    state.field = false;
    state.content = qApp->arguments().contains(QStringLiteral("--external"))
                        ? service_record()
                        : manager::application::nfc::make_uri_record("https://example.org/");
    return state;
}
class PreviewClient final : public manager::application::nfc::ClientPort {
  public:
    void observe(std::function<void(const manager::application::nfc::Snapshot &)> observer) override {
        if (observer)
            observer(fixture());
    }
    void refresh() override {}
    bool write(manager::application::nfc::WriteRequest,
               std::function<void(manager::application::nfc::Status)>) override {
        return false;
    }
};
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
bool apply_record(QObject &record, int index, const QString &value) {
    if (!QMetaObject::invokeMethod(&record, "edit", Q_ARG(int, -1)))
        return false;
    auto *format = record.findChild<QObject *>(QStringLiteral("nfcRecordType"));
    auto *field = record.findChild<QObject *>(QStringLiteral("nfcRecordValue"));
    format->setProperty("currentIndex", index);
    field->setProperty("text", value);
    bool accepted{};
    return field->property("textFormat").toInt() == Qt::PlainText &&
           QMetaObject::invokeMethod(&record, "apply", Q_RETURN_ARG(bool, accepted)) && accepted;
}
bool edited_record(QObject &record, const QString &value) {
    if (!QMetaObject::invokeMethod(&record, "edit", Q_ARG(int, 1)))
        return false;
    auto *field = record.findChild<QObject *>(QStringLiteral("nfcRecordValue"));
    if (field->property("text").toString() != value)
        return false;
    field->setProperty("text", value + QStringLiteral("!"));
    bool accepted{};
    return QMetaObject::invokeMethod(&record, "apply", Q_RETURN_ARG(bool, accepted)) && accepted;
}
bool kept_service(manager::presentation::NfcModel &model) {
    const auto raw = QByteArray::fromHex(model.draftHex().toLatin1());
    const manager::application::nfc::Bytes bytes(raw.begin(), raw.end());
    const auto decoded = manager::application::nfc::decode_ndef(bytes);
    const auto original = manager::application::nfc::decode_ndef(service_record());
    return decoded && decoded->size() == 3 && original && decoded->front() == original->front();
}
void check_editor(QQuickItem &item) {
    auto *editor = item.findChild<QObject *>(QStringLiteral("nfcEditor"));
    auto *record = editor->findChild<QObject *>(QStringLiteral("nfcRecordEditor"));
    auto *model = qobject_cast<manager::presentation::NfcModel *>(item.property("nfc").value<QObject *>());
    auto state = fixture();
    state.content = service_record();
    model->acceptState(state);
    const auto text = QString::fromUtf8("Привет <b>обычный текст</b>");
    const bool opened = QMetaObject::invokeMethod(editor, "edit");
    const bool passed =
        opened && !model->draftDirty() && apply_record(*record, 0, text) && edited_record(*record, text) &&
        apply_record(*record, 1, QStringLiteral("https://example.org/")) && kept_service(*model);
    if (!passed) {
        std::cerr << "NFC record-list UI regression failed\n";
        QCoreApplication::exit(5);
    }
}
void show_dialog(QQuickItem &item) {
    if (qApp->arguments().contains(QStringLiteral("--editor-check"))) {
        check_editor(item);
        return;
    }
    if (qApp->arguments().contains(QStringLiteral("--editor")))
        QMetaObject::invokeMethod(item.findChild<QObject *>(QStringLiteral("nfcEditor")), "edit");
}
void size_preview(QQuickWindow &window, QQuickItem &item) {
    item.setParentItem(window.contentItem());
    const int width = qApp->arguments().contains(QStringLiteral("--narrow")) ? 440 : 740;
    item.setWidth(width);
    item.setHeight(qApp->arguments().contains(QStringLiteral("--card"))   ? item.implicitHeight()
                   : qApp->arguments().contains(QStringLiteral("--tall")) ? 680
                                                                          : 520);
    window.resize(width, static_cast<int>(item.height()));
    if (qApp->arguments().contains(QStringLiteral("--card")))
        QObject::connect(&item, &QQuickItem::implicitHeightChanged, &window, [&] {
            item.setHeight(item.implicitHeight());
            window.resize(static_cast<int>(item.width()), static_cast<int>(item.height()));
        });
}
void attach(QQuickWindow &window, QQuickItem &item, const QString &path) {
    size_preview(window, item);
    QObject::connect(&window, &QQuickWindow::frameSwapped, qApp, [&, path, armed = false]() mutable {
        if (!std::exchange(armed, true))
            QMetaObject::invokeMethod(qApp, [&, path] {
                show_dialog(item);
                QTimer::singleShot(250, qApp, [&, path] { capture(*window.contentItem(), path); });
            });
    });
    QTimer::singleShot(5000, qApp, [] { QCoreApplication::exit(3); });
}
int run_preview(QApplication &app, const QString &path, QTranslator &translator,
                manager::presentation::ThemeController &theme) {
    configure_appearance(app, translator, theme);
    manager::presentation::NfcModel model(std::make_shared<PreviewClient>());
    QQmlEngine engine;
    const auto object = create_card(engine, model);
    auto *item = qobject_cast<QQuickItem *>(object.get());
    if (!item)
        return 2;
    engine.retranslate();
    QQuickWindow window;
    window.setColor(QGuiApplication::palette().color(QPalette::Window));
    attach(window, *item, path);
    window.show();
    return app.exec();
}
} // namespace
int main(int argc, char **argv) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    const bool fluent =
        std::any_of(argv, argv + argc, [](const char *arg) { return qstrcmp(arg, "--fluent") == 0; });
    QQuickStyle::setStyle(fluent ? QStringLiteral("FluentWinUI3") : QStringLiteral("Fusion"));
    QApplication app(argc, argv);
    app.setStyle(QStringLiteral("Fusion"));
    if (argc < 2)
        return 1;
    QTranslator translator;
    manager::presentation::ThemeController theme;
    return run_preview(app, QString::fromLocal8Bit(argv[1]), translator, theme);
}
