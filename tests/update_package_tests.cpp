#include "adapters/updates/download_job.hpp"
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QFile>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QTemporaryDir>
#include <QTimer>
#include <algorithm>
#include <cstring>

namespace {
namespace app = manager::application;
namespace adapters = manager::adapters;
class Reply final : public QNetworkReply {
  public:
    Reply(const QNetworkRequest &request, QByteArray bytes, bool fail, bool stall, QObject *parent)
        : QNetworkReply(parent), bytes_(std::move(bytes)) {
        setRequest(request);
        setUrl(request.url());
        setAttribute(QNetworkRequest::HttpStatusCodeAttribute, 200);
        open(QIODevice::ReadOnly);
        if (!stall)
            QTimer::singleShot(0, this, [this, fail] {
                if (fail)
                    setError(QNetworkReply::RemoteHostClosedError, QStringLiteral("disconnected"));
                emit readyRead();
                finish();
            });
    }
    void abort() override {
        setError(QNetworkReply::OperationCanceledError, QStringLiteral("cancelled"));
        finish();
    }
    qint64 bytesAvailable() const override {
        return bytes_.size() - position_ + QNetworkReply::bytesAvailable();
    }

  private:
    void finish() {
        if (!isFinished()) {
            setFinished(true);
            emit finished();
        }
    }
    qint64 readData(char *destination, qint64 maximum) override {
        const auto count = std::min(maximum, static_cast<qint64>(bytes_.size() - position_));
        if (count == 0)
            return -1;
        std::memcpy(destination, bytes_.constData() + position_, static_cast<std::size_t>(count));
        position_ += count;
        return count;
    }
    QByteArray bytes_;
    qint64 position_{};
};
class Network final : public QNetworkAccessManager {
  public:
    QByteArray bytes{"installer fixture"};
    bool fail{};
    bool stall{};

  private:
    QNetworkReply *createRequest(Operation, const QNetworkRequest &request, QIODevice *) override {
        return new Reply(request, bytes, fail, stall, this);
    }
};
app::UpdateResult release(const QByteArray &bytes) {
    return {.state = app::UpdateState::Available,
            .version = "999.0.0",
            .release_url = "https://github.com/KobaProduction/redmibook-manager/releases/tag/v999.0.0",
            .download_url = "https://github.com/KobaProduction/redmibook-manager/releases/download/v999.0.0/"
                            "RedmibookManager-999.0.0-x86_64.exe",
            .download_sha256 =
                QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex().toStdString(),
            .download_size = static_cast<std::uint64_t>(bytes.size())};
}
bool verified_download() {
    Network network;
    QTemporaryDir directory;
    const auto progress = std::make_shared<adapters::DownloadProgress>();
    std::uint64_t received{};
    QObject::connect(progress.get(), &adapters::DownloadProgress::progressed, progress.get(),
                     [&](quint64 count, quint64) { received = count; });
    const auto package = adapters::fetch_package(release(network.bytes), directory.path(), progress, network);
    QFile file(package.path);
    return received == static_cast<std::uint64_t>(network.bytes.size()) && file.open(QIODevice::ReadOnly) &&
           file.readAll() == network.bytes && package.size == received;
}
struct DownloadCase {
    Network network;
    QTemporaryDir directory;
    std::shared_ptr<adapters::DownloadProgress> progress{std::make_shared<adapters::DownloadProgress>()};
    app::UpdateResult expected{release(network.bytes)};
    bool rejected() {
        return adapters::fetch_package(expected, directory.path(), progress, network).path.isEmpty();
    }
};
bool rejected_integrity() {
    DownloadCase test;
    test.expected.download_sha256.assign(64, '0');
    if (!test.rejected())
        return false;
    test.expected = release(test.network.bytes);
    --test.expected.download_size;
    return test.rejected();
}
bool rejected_transport() {
    DownloadCase test;
    test.network.fail = true;
    return test.rejected();
}
bool cancelled_download() {
    DownloadCase test;
    test.network.stall = true;
    QTimer::singleShot(0, test.progress.get(), [progress = test.progress] { progress->requestCancel(); });
    return test.rejected();
}
bool rejected_downloads() {
    return rejected_integrity() && rejected_transport() && cancelled_download();
}
bool metadata_and_tampering() {
    auto expected = release(QByteArray("fixture"));
    if (!adapters::valid_package(expected))
        return false;
    expected.download_url = "https://example.com/RedmibookManager-999.0.0-x86_64.exe";
    if (adapters::valid_package(expected))
        return false;
    QTemporaryDir directory;
    QFile file(directory.filePath(QStringLiteral("corrupted.exe")));
    if (!file.open(QIODevice::WriteOnly) || file.write("fixture") != 7)
        return false;
    file.close();
    return adapters::launch_update({file.fileName(), QByteArray(64, '0'), 7}) == app::InstallResult::Failed;
}
} // namespace
int main(int argc, char *argv[]) {
    QCoreApplication application(argc, argv);
    return verified_download() && rejected_downloads() && metadata_and_tampering() ? 0 : 1;
}
