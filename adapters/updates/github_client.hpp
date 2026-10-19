#pragma once
#include "application/updates/update_port.hpp"
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QUrl>

namespace manager::adapters {
bool trusted_release_url(const QUrl &url);
application::UpdateResult decode_release(const QJsonObject &object, const QString &target);
class GitHubUpdates final : public QObject, public application::UpdatePort {
  public:
    explicit GitHubUpdates(QString target);
    ~GitHubUpdates() override;
    void check(std::function<void(application::UpdateResult)> completion) override;

  private:
    QNetworkAccessManager network_;
    QString target_;
};
} // namespace manager::adapters
