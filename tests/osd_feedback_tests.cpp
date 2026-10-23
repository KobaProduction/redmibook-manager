#include "adapters/local_api/feedback_channel.hpp"
#include "adapters/local_api/session_server.hpp"
#include "adapters/storage/ui_preferences/store.hpp"
#include "presentation/osd/osd_presenter.hpp"
#include <QApplication>
#include <QLabel>
#include <QTemporaryDir>
#include <QTimer>
#include <array>
namespace {
bool wire_contract() {
    using namespace manager::application::feedback;
    using namespace manager::adapters;
    const auto frame = encode_feedback({Kind::Performance, 3});
    const auto event = decode_feedback(frame);
    auto incompatible = frame;
    incompatible.replace(MANAGER_BUILD_ID, "different-build");
    return event && event->kind == Kind::Performance && event->value == 3 && !decode_feedback(incompatible) &&
           !decode_feedback(encode_feedback({Kind::Performance, 4})) && !decode_feedback(frame + " extra") &&
           !decode_feedback("RMB/1 feedback invalid");
}
bool verify_events(manager::presentation::OsdPresenter &osd, const QLabel &icon, const QLabel &text) {
    using namespace manager::application::feedback;
    const std::array events{Event{Kind::Microphone, 0},  Event{Kind::Microphone, 1},
                            Event{Kind::Performance, 1}, Event{Kind::Performance, 2},
                            Event{Kind::Performance, 3}, Event{Kind::Refresh, 60},
                            Event{Kind::Refresh, 120}};
    for (const auto event : events) {
        if (osd.present(event) != Delivery::Presented || icon.pixmap().isNull() || text.text().isEmpty() ||
            osd.present(event) != Delivery::Suppressed)
            return false;
    }
    return true;
}
bool shared_surface(manager::presentation::Preferences &preferences) {
    using namespace manager::application::feedback;
    manager::presentation::OsdPresenter osd(preferences);
    auto *icon = osd.findChild<QLabel *>(QStringLiteral("osdIcon"));
    auto *text = osd.findChild<QLabel *>(QStringLiteral("osdText"));
    if (!icon || !text || !verify_events(osd, *icon, *text))
        return false;
    osd.show_result({true, {}, {}, 0});
    if (!osd.isVisible() || icon->pixmap().isNull() || !text->text().contains("Diagnostic"))
        return false;
    osd.prepare_refresh();
    osd.present_refresh({manager::application::display::Status::Ready, 165, {}});
    if (!osd.isVisible() || !text->text().contains("165"))
        return false;
    osd.present_mi();
    return !icon->pixmap().isNull() && text->text().contains("Mi") &&
           osd.present(Event{Kind::Performance, 0}) == Delivery::Invalid;
}
bool independent_feedback(manager::presentation::Preferences &preferences) {
    manager::presentation::OsdPresenter osd(preferences);
    if (!preferences.set_osd_sound(true) || !preferences.set_osd_enabled(false))
        return false;
    osd.show_result({});
    if (osd.isVisible() || !preferences.osd_sound())
        return false;
    if (!preferences.set_osd_enabled(true) || !preferences.set_osd_sound(false))
        return false;
    osd.show_result({});
    if (!osd.isVisible())
        return false;
    return preferences.set_osd_enabled(false) && !osd.isVisible() && preferences.set_osd_enabled(true);
}
class DiagnosticFake final : public manager::application::DiagnosticPort {
  public:
    mutable int calls{};
    manager::application::DiagnosticReply probe() const override {
        ++calls;
        return {};
    }
};
class DeliveryProbe final : public manager::application::feedback::Presenter {
  public:
    explicit DeliveryProbe(manager::presentation::OsdPresenter &osd) : osd_(osd) {}
    manager::application::feedback::Delivery present(manager::application::feedback::Event event) override {
        const auto result = osd_.present(event);
        const bool correct = event.kind == manager::application::feedback::Kind::Refresh &&
                             event.value == 165 &&
                             result == manager::application::feedback::Delivery::Presented;
        QTimer::singleShot(0, qApp, [correct] { QCoreApplication::exit(correct ? 0 : 1); });
        return result;
    }

  private:
    manager::presentation::OsdPresenter &osd_;
};
int transport_contract(QApplication &app, manager::presentation::Preferences &preferences) {
    qputenv("MANAGER_DEV_SCOPE",
            QByteArray("feedback-test-") + QByteArray::number(QCoreApplication::applicationPid()));
    DiagnosticFake native;
    const manager::application::ReadRuntimeStatus read(native);
    manager::presentation::OsdPresenter osd(preferences);
    DeliveryProbe feedback(osd);
    manager::adapters::SessionServer server(read, osd, &feedback);
    manager::adapters::FeedbackClient client;
    if (!server.listen() || client.present({manager::application::feedback::Kind::Refresh, 165}) !=
                                manager::application::feedback::Delivery::Queued)
        return 1;
    QTimer::singleShot(2500, &app, [] { QCoreApplication::exit(1); });
    const auto result = app.exec();
    return result == 0 && native.calls == 0 ? 0 : 1;
}
} // namespace
int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    QTemporaryDir directory;
    manager::adapters::UiPreferenceStore store(directory.path());
    if (!store.update("language", "en") || !store.update("osdSound", "false"))
        return 1;
    manager::presentation::Preferences preferences(store);
    if (!wire_contract() || !shared_surface(preferences) || !independent_feedback(preferences))
        return 1;
    return transport_contract(app, preferences);
}
