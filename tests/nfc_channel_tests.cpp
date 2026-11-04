#include "adapters/local_api/nfc_client.hpp"
#include "adapters/local_api/nfc_codec.hpp"
#include "adapters/local_api/nfc_server.hpp"
#include "application/nfc/ndef.hpp"
#include "presentation/nfc_model.hpp"
#include "presentation/nfc_records.hpp"
#include <QCoreApplication>
#include <QEventLoop>
#include <QTimer>
#include <atomic>
#include <thread>

namespace {
namespace app = manager::application::nfc;
class Tag final : public app::TagPort {
  public:
    app::Snapshot state;
    const std::thread::id owner{std::this_thread::get_id()};
    std::atomic<bool> gui_read{};
    std::atomic<int> writes{};
    Tag() {
        state.status = app::Status::Ready;
        state.present = true;
        state.writable = true;
        state.field = false;
        state.capacity = 1024;
        state.max_content_size = 1020;
        state.name = "NFC fixture";
        state.content = app::make_text_record("Original");
    }
    app::Snapshot read() override {
        if (owner == std::this_thread::get_id())
            gui_read = true;
        return state;
    }
    app::Result write(const app::WriteRequest &request) override {
        if (state.content != request.expected)
            return {app::Status::Conflict, state};
        ++writes;
        state.content = request.content;
        return {app::Status::Ready, state};
    }
    std::unique_ptr<app::Subscription> subscribe(std::function<void()>) override {
        return {};
    }
};
bool await(manager::presentation::NfcModel &model, const std::function<bool()> &condition) {
    if (condition())
        return true;
    QEventLoop loop;
    QTimer deadline;
    deadline.setSingleShot(true);
    QObject::connect(&deadline, &QTimer::timeout, &loop, &QEventLoop::quit);
    QObject::connect(&model, &manager::presentation::NfcModel::changed, &loop, [&] {
        if (condition())
            loop.quit();
    });
    deadline.start(2500);
    loop.exec();
    return condition();
}
bool exercise(manager::presentation::NfcModel &model, const std::shared_ptr<Tag> &tag) {
    if (!await(model, [&] { return model.writable(); }) || !model.beginDraft())
        return false;
    if (model.putDraftRecord(0, "text", QString(2100, QLatin1Char('x'))) || tag->writes != 0 || model.busy())
        return false;
    if (!model.putDraftRecord(0, "text", QStringLiteral("Changed")))
        return false;
    model.saveDraft();
    return await(model, [&] { return !model.busy() && tag->writes == 1; }) && model.records().size() == 1;
}
bool reject_stale_draft(manager::presentation::NfcModel &model, const std::shared_ptr<Tag> &tag) {
    if (!model.beginDraft() || !model.putDraftRecord(0, "text", QStringLiteral("Stale draft")))
        return false;
    auto external = tag->state;
    external.content = app::make_text_record("External change");
    model.acceptState(external);
    model.saveDraft();
    return !model.busy() && tag->writes == 1;
}
bool reject_forged(manager::adapters::NfcClient &client) {
    QEventLoop loop;
    QTimer deadline;
    deadline.setSingleShot(true);
    QObject::connect(&deadline, &QTimer::timeout, &loop, &QEventLoop::quit);
    auto status = app::Status::Unavailable;
    if (!client.write({app::make_text_record("Forged"), app::make_text_record("New")}, [&](auto result) {
            status = result;
            loop.quit();
        }))
        return false;
    deadline.start(2500);
    loop.exec();
    return status == app::Status::Conflict;
}
bool record_display() {
    const auto uri = manager::presentation::nfcRecords(
        {0xd1, 1, 12, 'U', 4, 'e', 'x', 'a', 'm', 'p', 'l', 'e', '.', 'o', 'r', 'g'});
    const auto text = manager::presentation::nfcRecords({0xd1, 1, 3, 'T', 0x80, 0, 'A'});
    return uri.size() == 1 &&
           uri[0].toMap().value("value").toString() == QStringLiteral("https://example.org") &&
           text.size() == 1 && text[0].toMap().value("value").toString() == QStringLiteral("A");
}
bool channel_contract() {
    QThreadPool pool;
    pool.setMaxThreadCount(1);
    const auto tag = std::make_shared<Tag>();
    const auto endpoint =
        QStringLiteral("Redmibook.Nfc.Test.") + QString::number(QCoreApplication::applicationPid());
    manager::adapters::NfcServer server(tag, pool, endpoint);
    if (!server.listen())
        return false;
    auto client = std::make_shared<manager::adapters::NfcClient>(endpoint);
    manager::presentation::NfcModel model(client);
    if (!await(model, [&] { return model.writable(); }) || !reject_forged(*client))
        return false;
    if (!exercise(model, tag) || tag->gui_read)
        return false;
    return reject_stale_draft(model, tag);
}

bool capability_codec() {
    app::Snapshot state;
    state.status = app::Status::DriverMissing;
    state.present = true;
    const auto parsed =
        manager::adapters::nfc_wire::parse_snapshot(manager::adapters::nfc_wire::snapshot(state));
    if (!parsed || *parsed != state)
        return false;
    state.capacity = 1024;
    state.max_content_size = 1020;
    const auto ready =
        manager::adapters::nfc_wire::parse_snapshot(manager::adapters::nfc_wire::snapshot(state));
    state.max_content_size = 1025;
    return ready && ready->max_content_size == 1020 &&
           !manager::adapters::nfc_wire::parse_snapshot(manager::adapters::nfc_wire::snapshot(state));
}
bool incompatible_envelope() {
    auto object = manager::adapters::nfc_wire::envelope("state");
    object.insert("build", "other-build");
    return !manager::adapters::nfc_wire::compatible(object) &&
           !manager::adapters::nfc_wire::parse_bytes(QStringLiteral("invalid%base64"));
}
} // namespace
int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    return channel_contract() && incompatible_envelope() && capability_codec() && record_display() ? 0 : 1;
}
