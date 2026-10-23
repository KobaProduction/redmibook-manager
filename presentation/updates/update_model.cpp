#include "presentation/updates/update_model.hpp"
#include <QPointer>
namespace manager::presentation {
UpdateModel::UpdateModel(application::UpdatePort &source, QString target)
    : source_(source), target_(std::move(target)) {}
QString UpdateModel::status() const {
    if (busy_) {
        return tr("Checking for updates…");
    }
    if (!checked_) {
        return tr("Not checked yet.");
    }
    switch (result_.state) {
    case application::UpdateState::Available:
        return tr("Version %1 is available.").arg(QString::fromStdString(result_.version));
    case application::UpdateState::Current:
        return tr("You have the latest version in your update channel.");
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
    if (busy_) {
        return;
    }
    busy_ = true;
    emit changed();
    const QPointer<UpdateModel> receiver(this);
    source_.check([receiver](const application::UpdateResult &result) {
        if (receiver) {
            receiver->result_ = result;
            receiver->checked_ = true;
            receiver->checked_at_ = QDateTime::currentDateTime();
            receiver->busy_ = false;
            emit receiver->changed();
        }
    });
}
} // namespace manager::presentation
