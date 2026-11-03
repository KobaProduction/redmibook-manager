#include "adapters/local_api/nfc_client.hpp"
#include "adapters/local_api/nfc_codec.hpp"
#include "adapters/local_api/nfc_server.hpp"
#include "application/nfc/ndef.hpp"
#include "presentation/nfc_model.hpp"
#include <QCoreApplication>
#include <QDir>
#include <QEventLoop>
#include <QTemporaryDir>
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
        state.capacity = 1024;
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
    if (!await(model, [&] { return model.writable(); }))
        return false;
    model.writeText(QString(2100, QLatin1Char('x')));
    if (tag->writes != 0 || model.busy())
        return false;
    model.writeText(QStringLiteral("Changed"));
    return await(model, [&] { return !model.busy() && tag->writes == 1; }) && model.records().size() == 1;
}
bool channel_contract() {
    QTemporaryDir directory;
    QThreadPool pool;
    pool.setMaxThreadCount(1);
    const auto tag = std::make_shared<Tag>();
    const auto endpoint =
        QStringLiteral("Redmibook.Nfc.Test.") + QString::number(QCoreApplication::applicationPid());
    manager::adapters::NfcServer server(tag, pool, endpoint, directory.path());
    if (!server.listen())
        return false;
    auto client = std::make_shared<manager::adapters::NfcClient>(endpoint);
    manager::presentation::NfcModel model(client);
    return exercise(model, tag) && !tag->gui_read &&
           QDir(directory.path()).entryList({"*.ndef"}, QDir::Files).size() == 1;
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
    return channel_contract() && incompatible_envelope() ? 0 : 1;
}
