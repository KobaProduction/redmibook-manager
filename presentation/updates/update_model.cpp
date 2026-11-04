#include "presentation/updates/update_model.hpp"
#include <QPointer>
namespace manager::presentation {
UpdateModel::UpdateModel(application::UpdatePort &source, QString target,
                         application::UpdatePackagePort *packages)
    : source_(source), packages_(packages), target_(std::move(target)) {}
QString UpdateModel::status() const {
    if (transfer_ != Transfer::Idle)
        return transferStatus();
    if (!checked_) {
        return tr("Not checked yet.");
    }
    switch (result_.state) {
    case application::UpdateState::Available:
        if (packages_ && (!directDownload() || result_.download_sha256.empty() || result_.download_size == 0))
            return tr("Update package unavailable. Try again later.");
        return tr("Version %1 is available.").arg(QString::fromStdString(result_.version));
    case application::UpdateState::Current:
        return tr("Up to date");
    case application::UpdateState::Unavailable:
        return tr("No public release is available.");
    case application::UpdateState::Failed:
        return tr("Could not check for updates. Try again later.");
    }
    return {};
}
QString UpdateModel::downloadUrl() const {
    return QString::fromStdString(result_.download_url.empty() ? result_.release_url : result_.download_url);
}
void UpdateModel::check() {
    if (busy()) {
        return;
    }
    busy_ = true;
    emit changed();
    const QPointer<UpdateModel> receiver(this);
    source_.check([receiver](const application::UpdateResult &result) {
        if (receiver) {
            receiver->transfer_ = Transfer::Idle;
            receiver->progress_ = 0;
            receiver->result_ = result;
            receiver->checked_ = true;
            receiver->checked_at_ = QDateTime::currentDateTime();
            receiver->busy_ = false;
            emit receiver->changed();
        }
    });
}
} // namespace manager::presentation
