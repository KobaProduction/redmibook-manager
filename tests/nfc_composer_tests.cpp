#include "adapters/storage/nfc_files.hpp"
#include "presentation/nfc_model.hpp"
#include <QCoreApplication>
#include <QEventLoop>
#include <QFile>
#include <QTemporaryDir>
#include <QTimer>
#include <iostream>
#include <stdexcept>

namespace {
namespace app = manager::application::nfc;
class Client final : public app::ClientPort {
  public:
    app::Snapshot state;
    std::function<void(const app::Snapshot &)> observer;
    int writes{};
    app::WriteRequest last;
    Client() {
        state.status = app::Status::Ready;
        state.present = state.writable = true;
        state.capacity = 1024;
        state.max_content_size = 1020;
        state.field = false;
        auto encoded =
            app::encode_ndef(std::vector{app::Record{4, {'x', ':', 'v'}, {0, 255}, {0, 128, 255}}});
        if (!encoded)
            throw std::runtime_error("Invalid fixture");
        state.content = std::move(*encoded);
    }
    void observe(std::function<void(const app::Snapshot &)> callback) override {
        observer = std::move(callback);
        if (observer)
            observer(state);
    }
    void refresh() override {}
    bool write(app::WriteRequest request, std::function<void(app::Status)> callback) override {
        ++writes;
        last = std::move(request);
        state.content = last.content;
        observer(state);
        callback(app::Status::Ready);
        return true;
    }
};
bool preserve_and_save() {
    const auto client = std::make_shared<Client>();
    const auto original = client->state.content;
    manager::presentation::NfcModel model(client);
    model.beginDraft();
    if (model.draftDirty() || !model.putDraftRecord(-1, "text", QString::fromUtf8("Текст")) ||
        !model.putDraftRecord(-1, "phone", "+79991234567") ||
        !model.putDraftRecord(-1, "email", "name@example.org"))
        return false;
    model.moveDraftRecord(3, 2);
    model.duplicateDraftRecord(1);
    model.removeDraftRecord(2);
    if (client->writes != 0 || model.draftRecords().size() != 4 || !model.draftFits())
        return false;
    model.saveDraft();
    const auto saved = app::decode_ndef(client->last.content);
    const auto initial = app::decode_ndef(original);
    return client->writes == 1 && client->last.expected == original && saved && saved->size() == 4 &&
           initial && saved->front() == initial->front();
}
bool conflict_and_cancel() {
    const auto client = std::make_shared<Client>();
    manager::presentation::NfcModel model(client);
    model.beginDraft();
    model.putDraftRecord(-1, "text", "Draft");
    auto external = client->state;
    external.content = app::make_text_record("External");
    model.acceptState(external);
    if (!model.draftConflict() || model.draftRecords().size() != 2)
        return false;
    model.saveDraft();
    if (client->writes != 0)
        return false;
    model.endDraft();
    return !model.putDraftRecord(-1, "text", "After close") && client->writes == 0;
}
bool field_notifications(manager::presentation::NfcModel &model, app::Snapshot field) {
    int changes{};
    const auto connection =
        QObject::connect(&model, &manager::presentation::NfcModel::draftChanged, [&] { ++changes; });
    field.field = true;
    model.acceptState(field);
    field.field = false;
    model.acceptState(field);
    QObject::disconnect(connection);
    return changes == 0 && !model.draftDirty();
}
bool capacity_and_notifications() {
    const auto client = std::make_shared<Client>();
    manager::presentation::NfcModel model(client);
    model.beginDraft();
    if (!field_notifications(model, client->state))
        return false;
    model.putDraftRecord(-1, "text", QString(1100, 'x'));
    if (model.draftFits())
        return false;
    model.saveDraft();
    model.clearDraft();
    model.saveDraft();
    return client->writes == 1 && client->last.content.empty();
}
bool wait_idle(manager::presentation::NfcModel &model) {
    if (!model.busy())
        return true;
    QEventLoop loop;
    QTimer deadline;
    deadline.setSingleShot(true);
    QObject::connect(&deadline, &QTimer::timeout, &loop, &QEventLoop::quit);
    QObject::connect(&model, &manager::presentation::NfcModel::changed, &loop, [&] {
        if (!model.busy())
            loop.quit();
    });
    deadline.start(2500);
    loop.exec();
    return !model.busy();
}
class DeferredFiles final : public app::FilePort {
  public:
    Completion completion;
    bool load(std::string, Completion callback) override {
        completion = std::move(callback);
        return true;
    }
    bool save(std::string, app::Bytes, Completion) override {
        return false;
    }
};
bool late_file_result() {
    const auto client = std::make_shared<Client>();
    const auto files = std::make_shared<DeferredFiles>();
    auto model = std::make_unique<manager::presentation::NfcModel>(client, nullptr, files);
    const auto file = QUrl::fromLocalFile(QStringLiteral("late.ndef"));
    model->beginDraft();
    model->importDraft(file);
    if (!model->busy() || model->putDraftRecord(-1, "text", "while busy"))
        return false;
    model->endDraft();
    files->completion({app::Status::Ready, app::make_text_record("late")});
    if (model->busy() || !model->draftRecords().empty() || client->writes != 0)
        return false;
    model->beginDraft();
    model->importDraft(file);
    model.reset();
    files->completion({app::Status::Ready, app::make_text_record("after destruction")});
    return client->writes == 0;
}
bool reject_broken_file(manager::presentation::NfcModel &model, const QUrl &file, const QString &expected) {
    QFile broken(file.toLocalFile());
    if (!broken.open(QIODevice::WriteOnly | QIODevice::Truncate) || broken.write("broken") != 6)
        return false;
    broken.close();
    model.importDraft(file);
    return wait_idle(model) && model.draftHex() == expected;
}
bool file_round_trip() {
    const auto client = std::make_shared<Client>();
    manager::presentation::NfcModel model(client, nullptr, manager::adapters::make_nfc_files());
    QTemporaryDir directory;
    const auto file = QUrl::fromLocalFile(directory.filePath(QStringLiteral("draft.ndef")));
    model.beginDraft();
    model.putDraftRecord(-1, "uri", "https://example.org/");
    const auto expected = model.draftHex();
    model.exportDraft(file);
    if (!wait_idle(model))
        return false;
    model.clearDraft();
    model.importDraft(file);
    if (!wait_idle(model) || model.draftHex() != expected || client->writes != 0)
        return false;
    return reject_broken_file(model, file, expected) && client->writes == 0;
}
} // namespace
int main(int argc, char **argv) {
    QCoreApplication application(argc, argv);
    try {
        const bool passed = preserve_and_save() && conflict_and_cancel() && capacity_and_notifications() &&
                            late_file_result() && file_round_trip();
        if (!passed)
            std::cerr << "NFC composer contract failed\n";
        return passed ? 0 : 1;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
