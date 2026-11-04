#include "presentation/updates/update_model.hpp"
#include <QPointer>
#include <algorithm>

namespace manager::presentation {
bool UpdateModel::canDownload() const {
    return packages_ && available() && !busy() && directDownload() && !result_.download_sha256.empty() &&
           result_.download_size > 0 && !canInstall() && transfer_ != Transfer::Started;
}
bool UpdateModel::canInstall() const {
    return packages_ && !busy() && (transfer_ == Transfer::Ready || transfer_ == Transfer::Cancelled);
}
QString UpdateModel::transferStatus() const {
    switch (transfer_) {
    case Transfer::Downloading:
        return tr("Downloading…");
    case Transfer::Ready:
        return tr("Ready to install");
    case Transfer::Launching:
        return tr("Opening installer…");
    case Transfer::Started:
        return tr("Installer opened");
    case Transfer::DownloadFailed:
        return tr("Download failed. Try again.");
    case Transfer::InstallFailed:
        return tr("Could not open the installer. Download the update again.");
    case Transfer::Cancelled:
        return tr("Installation cancelled");
    case Transfer::Idle:
        return {};
    }
    return {};
}
void UpdateModel::download() {
    if (!canDownload())
        return;
    transfer_ = Transfer::Downloading;
    progress_ = 0;
    emit changed();
    const QPointer<UpdateModel> receiver(this);
    packages_->download(
        result_,
        [receiver](std::uint64_t received, std::uint64_t total) {
            if (!receiver || !receiver->downloading() || total == 0)
                return;
            receiver->progress_ =
                std::clamp(static_cast<double>(received) / static_cast<double>(total), 0.0, 1.0);
            emit receiver->changed();
        },
        [receiver](bool success) {
            if (receiver) {
                receiver->transfer_ = success ? Transfer::Ready : Transfer::DownloadFailed;
                emit receiver->changed();
            }
        });
}
void UpdateModel::install() {
    if (!canInstall())
        return;
    transfer_ = Transfer::Launching;
    emit changed();
    const QPointer<UpdateModel> receiver(this);
    packages_->install([receiver](application::InstallResult result) {
        if (!receiver)
            return;
        switch (result) {
        case application::InstallResult::Started:
            receiver->transfer_ = Transfer::Started;
            break;
        case application::InstallResult::Cancelled:
            receiver->transfer_ = Transfer::Cancelled;
            break;
        case application::InstallResult::Failed:
            receiver->transfer_ = Transfer::InstallFailed;
            break;
        }
        emit receiver->changed();
    });
}
} // namespace manager::presentation
