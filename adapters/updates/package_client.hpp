#pragma once
#include "adapters/updates/download_job.hpp"
#include <QFutureWatcher>

namespace manager::adapters {
class UpdatePackages final : public QObject, public application::UpdatePackagePort {
  public:
    explicit UpdatePackages(QString directory = {});
    ~UpdatePackages() override;
    void download(const application::UpdateResult &release, Progress progress,
                  std::function<void(bool)> completion) override;
    void install(std::function<void(application::InstallResult)> completion) override;

  private:
    QString directory_;
    DownloadedPackage package_;
    std::shared_ptr<DownloadProgress> progress_;
    QFutureWatcher<DownloadedPackage> download_;
    QFutureWatcher<application::InstallResult> install_;
    std::function<void(bool)> downloaded_;
    std::function<void(application::InstallResult)> installed_;
};
} // namespace manager::adapters
