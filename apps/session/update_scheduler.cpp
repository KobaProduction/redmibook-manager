#include "apps/session/update_scheduler.hpp"
#include "adapters/updates/target.hpp"
#include <QDesktopServices>
#include <QPointer>
namespace manager::session {
UpdateScheduler::UpdateScheduler(presentation::Preferences &preferences, const QString &directory)
    : preferences_(preferences), source_(adapters::update_target()), history_(directory),
      notice_(preferences) {
    connect(&notice_, &presentation::UpdateNotice::downloadRequested, this, [](const QString &url) {
        if (adapters::trusted_release_url(QUrl(url))) {
            QDesktopServices::openUrl(QUrl(url));
        }
    });
    connect(&timer_, &QTimer::timeout, this, &UpdateScheduler::check);
    timer_.start(3600000);
    QTimer::singleShot(5000, this, &UpdateScheduler::check);
}
void UpdateScheduler::check() {
    preferences_.refresh();
    if (!preferences_.auto_updates() || !history_.claim_daily_check()) {
        return;
    }
    const QPointer<UpdateScheduler> receiver(this);
    source_.check([receiver](const application::UpdateResult &result) {
        if (receiver && receiver->preferences_.auto_updates()) {
            receiver->notice_.show_result(result);
        }
    });
}
} // namespace manager::session
