#include "adapters/updates/download_job.hpp"
#include "adapters/updates/github_client.hpp"
#include <QCryptographicHash>
#include <QDir>
#include <QEventLoop>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QSaveFile>
#include <QTimer>
#include <algorithm>

namespace manager::adapters {
namespace {
constexpr std::uint64_t maximum_package = 256ULL * 1024 * 1024;
bool valid_digest(const std::string &digest) {
    return digest.size() == 64 && std::ranges::all_of(digest, [](char value) {
               return (value >= '0' && value <= '9') || (value >= 'a' && value <= 'f');
           });
}
} // namespace
bool valid_package(const application::UpdateResult &release) {
    const QUrl url(QString::fromStdString(release.download_url));
    const auto filename = url.fileName();
    return release.state == application::UpdateState::Available && trusted_release_url(url) &&
           url.path().contains(QStringLiteral("/releases/download/")) &&
           filename.startsWith(QStringLiteral("RedmibookManager-")) && !filename.contains(QLatin1Char('/')) &&
           !filename.contains(QLatin1Char('\\')) &&
           (filename.endsWith(QStringLiteral(".exe")) || filename.endsWith(QStringLiteral(".deb"))) &&
           release.download_size > 0 && release.download_size <= maximum_package &&
           valid_digest(release.download_sha256);
}
DownloadedPackage download_package(const application::UpdateResult &release, const QString &directory,
                                   const std::shared_ptr<DownloadProgress> &progress) {
    QNetworkAccessManager network;
    return fetch_package(release, directory, progress, network);
}
} // namespace manager::adapters
