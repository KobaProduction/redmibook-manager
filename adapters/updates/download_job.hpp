#pragma once
#include "application/updates/package_port.hpp"
#include <QObject>
#include <QString>
#include <atomic>
#include <memory>
class QNetworkAccessManager;

namespace manager::adapters {
struct DownloadedPackage {
    QString path;
    QByteArray digest;
    std::uint64_t size{};
};
class DownloadProgress final : public QObject {
    Q_OBJECT
  public:
    void requestCancel() {
        cancelled_.store(true);
        emit cancel();
    }
    bool cancelled() const {
        return cancelled_.load();
    }
  signals:
    void progressed(quint64 received, quint64 total);
    void cancel();

  private:
    std::atomic<bool> cancelled_{false};
};
bool valid_package(const application::UpdateResult &release);
DownloadedPackage fetch_package(const application::UpdateResult &release, const QString &directory,
                                const std::shared_ptr<DownloadProgress> &progress,
                                QNetworkAccessManager &network);
DownloadedPackage download_package(const application::UpdateResult &release, const QString &directory,
                                   const std::shared_ptr<DownloadProgress> &progress);
application::InstallResult launch_update(const DownloadedPackage &package);
} // namespace manager::adapters
