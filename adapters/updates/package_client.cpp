#include "adapters/updates/package_client.hpp"
#include <QDir>
#include <QStandardPaths>
#include <QtConcurrentRun>

namespace manager::adapters {
UpdatePackages::UpdatePackages(QString directory)
    : directory_(directory.isEmpty() ? QDir(QStandardPaths::writableLocation(QStandardPaths::CacheLocation))
                                           .filePath(QStringLiteral("updates"))
                                     : std::move(directory)) {
    connect(&download_, &QFutureWatcher<DownloadedPackage>::finished, this, [this] {
        try {
            package_ = download_.result();
        } catch (...) {
            package_ = {};
        }
        progress_.reset();
        auto completion = std::move(downloaded_);
        completion(!package_.path.isEmpty());
    });
    connect(&install_, &QFutureWatcher<application::InstallResult>::finished, this, [this] {
        application::InstallResult result;
        try {
            result = install_.result();
        } catch (...) {
            result = application::InstallResult::Failed;
        }
        auto completion = std::move(installed_);
        completion(result);
    });
}
UpdatePackages::~UpdatePackages() {
    if (progress_)
        progress_->requestCancel();
}
void UpdatePackages::download(const application::UpdateResult &release, Progress progress,
                              std::function<void(bool)> completion) {
    if (downloaded_ || installed_ || !valid_package(release)) {
        completion(false);
        return;
    }
    package_ = {};
    downloaded_ = std::move(completion);
    progress_ = std::shared_ptr<DownloadProgress>(new DownloadProgress,
                                                  [](DownloadProgress *value) { value->deleteLater(); });
    connect(progress_.get(), &DownloadProgress::progressed, this, std::move(progress));
    download_.setFuture(QtConcurrent::run([release, directory = directory_, progress = progress_] {
        return download_package(release, directory, progress);
    }));
}
void UpdatePackages::install(std::function<void(application::InstallResult)> completion) {
    if (downloaded_ || installed_ || package_.path.isEmpty()) {
        completion(application::InstallResult::Failed);
        return;
    }
    installed_ = std::move(completion);
    install_.setFuture(QtConcurrent::run([package = package_] { return launch_update(package); }));
}
} // namespace manager::adapters
