#include "adapters/updates/download_job.hpp"
#include <QCryptographicHash>
#include <QDir>
#include <QEventLoop>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QSaveFile>
#include <QTimer>

namespace manager::adapters {
namespace {
class Transfer final {
  public:
    Transfer(QNetworkReply &reply, QSaveFile &file, const application::UpdateResult &release,
             const std::shared_ptr<DownloadProgress> &progress)
        : reply_(reply), file_(file), release_(release) {
        reply_.setReadBufferSize(131072);
        deadline_.setSingleShot(true);
        QObject::connect(&deadline_, &QTimer::timeout, &reply_, &QNetworkReply::abort);
        QObject::connect(progress.get(), &DownloadProgress::cancel, &reply_, &QNetworkReply::abort);
        QObject::connect(&reply_, &QNetworkReply::finished, &loop_, &QEventLoop::quit);
        QObject::connect(&reply_, &QNetworkReply::readyRead, &loop_, [this, progress] {
            if (!drain())
                reply_.abort();
            const auto percent = received_ * 100 / release_.download_size;
            if (percent != percent_) {
                percent_ = percent;
                emit progress->progressed(received_, release_.download_size);
            }
        });
        if (progress->cancelled())
            reply_.abort();
    }
    DownloadedPackage run() {
        deadline_.start(300000);
        if (!reply_.isFinished())
            loop_.exec(); // Worker-local network event loop, never the GUI event loop.
        const bool drained = drain();
        const auto digest = hash_.result().toHex();
        if (!drained || reply_.error() != QNetworkReply::NoError ||
            reply_.attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() != 200 ||
            received_ != release_.download_size ||
            digest != QByteArray::fromStdString(release_.download_sha256) || !file_.commit())
            return {};
        return {file_.fileName(), digest, received_};
    }

  private:
    bool drain() {
        while (reply_.bytesAvailable() > 0) {
            const auto bytes = reply_.read(65536);
            received_ += static_cast<std::uint64_t>(bytes.size());
            if (bytes.isEmpty() || received_ > release_.download_size || file_.write(bytes) != bytes.size())
                return false;
            hash_.addData(bytes);
        }
        return true;
    }
    QNetworkReply &reply_;
    QSaveFile &file_;
    const application::UpdateResult &release_;
    QCryptographicHash hash_{QCryptographicHash::Sha256};
    QEventLoop loop_;
    QTimer deadline_;
    std::uint64_t received_{};
    std::uint64_t percent_{};
};
QString package_path(const application::UpdateResult &release, const QString &directory) {
    const auto folder = QDir(directory).filePath(QString::fromStdString(release.download_sha256));
    if (!QDir().mkpath(folder))
        return {};
    return QDir(folder).filePath(QUrl(QString::fromStdString(release.download_url)).fileName());
}
} // namespace
DownloadedPackage fetch_package(const application::UpdateResult &release, const QString &directory,
                                const std::shared_ptr<DownloadProgress> &progress,
                                QNetworkAccessManager &network) {
    if (!valid_package(release) || progress->cancelled())
        return {};
    const auto path = package_path(release, directory);
    QSaveFile file(path);
    if (path.isEmpty() || !file.open(QIODevice::WriteOnly))
        return {};
    QNetworkRequest request(QUrl(QString::fromStdString(release.download_url)));
    request.setTransferTimeout(15000);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    auto *reply = network.get(request);
    Transfer transfer(*reply, file, release, progress);
    const auto result = transfer.run();
    reply->deleteLater();
    return result;
}
} // namespace manager::adapters
