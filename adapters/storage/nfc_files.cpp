#include "adapters/storage/nfc_files.hpp"
#include <QFile>
#include <QFutureWatcher>
#include <QObject>
#include <QSaveFile>
#include <QTimer>
#include <QtConcurrentRun>

namespace manager::adapters {
namespace {
namespace app = application::nfc;
app::FileResult load_file(const std::string &path) {
    QFile file(QString::fromStdString(path));
    if (!file.open(QIODevice::ReadOnly) || file.size() > 2048)
        return {app::Status::Failed, {}};
    const auto bytes = file.read(2049);
    if (file.error() != QFileDevice::NoError || bytes.size() > 2048)
        return {app::Status::InvalidData, {}};
    return {app::Status::Ready, app::Bytes(bytes.begin(), bytes.end())};
}
app::FileResult save_file(const std::string &path, const app::Bytes &content) {
    QSaveFile file(QString::fromStdString(path));
    const auto size = static_cast<qint64>(content.size());
    if (content.size() > 2048 || !file.open(QIODevice::WriteOnly) ||
        file.write(reinterpret_cast<const char *>(content.data()), size) != size || !file.commit())
        return {app::Status::Failed, {}};
    return {app::Status::Ready, {}};
}
class NfcFiles final : public QObject, public app::FilePort {
  public:
    NfcFiles();
    bool load(std::string path, Completion completion) override {
        return start([path = std::move(path)] { return load_file(path); }, std::move(completion),
                     app::Status::TimedOut);
    }
    bool save(std::string path, app::Bytes content, Completion completion) override {
        return start(
            [path = std::move(path), content = std::move(content)] { return save_file(path, content); },
            std::move(completion), app::Status::UnknownOutcome);
    }

  private:
    QFutureWatcher<app::FileResult> watcher_;
    QTimer deadline_;
    Completion completion_;
    app::Status timeout_{app::Status::TimedOut};
    bool pending_{};
    bool start(std::function<app::FileResult()> operation, Completion completion, app::Status timeout);
    void deliver(app::FileResult result);
};
NfcFiles::NfcFiles() {
    deadline_.setSingleShot(true);
    deadline_.setInterval(10000);
    connect(&deadline_, &QTimer::timeout, this, [this] { deliver({timeout_, {}}); });
    connect(&watcher_, &QFutureWatcher<app::FileResult>::finished, this, [this] {
        deadline_.stop();
        pending_ = false;
        deliver(watcher_.result());
    });
}
bool NfcFiles::start(std::function<app::FileResult()> operation, Completion completion, app::Status timeout) {
    if (pending_)
        return false;
    pending_ = true;
    timeout_ = timeout;
    completion_ = std::move(completion);
    deadline_.start();
    watcher_.setFuture(QtConcurrent::run(std::move(operation)));
    return true;
}
void NfcFiles::deliver(app::FileResult result) {
    auto completion = std::move(completion_);
    completion_ = {};
    if (completion)
        completion(std::move(result));
}
} // namespace
std::shared_ptr<application::nfc::FilePort> make_nfc_files() {
    return std::make_shared<NfcFiles>();
}
} // namespace manager::adapters
