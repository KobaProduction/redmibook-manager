#pragma once
#include "adapters/updates/github_client.hpp"
#include "adapters/updates/history.hpp"
#include "presentation/updates/update_notice.hpp"
#include <QTimer>
namespace manager::session {
class UpdateScheduler final : public QObject {
  public:
    UpdateScheduler(presentation::Preferences &preferences, const QString &directory);

  private:
    presentation::Preferences &preferences_;
    adapters::GitHubUpdates source_;
    adapters::UpdateHistory history_;
    presentation::UpdateNotice notice_;
    QTimer timer_;
    void check();
};
} // namespace manager::session
