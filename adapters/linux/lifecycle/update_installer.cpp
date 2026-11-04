#include "adapters/updates/download_job.hpp"
#include <QCryptographicHash>
#include <QFile>
#include <QProcess>

namespace manager::adapters {
application::InstallResult launch_update(const DownloadedPackage &package) {
    QFile file(package.path);
    if (!file.open(QIODevice::ReadOnly) || file.size() < 0 ||
        static_cast<std::uint64_t>(file.size()) != package.size)
        return application::InstallResult::Failed;
    QCryptographicHash hash(QCryptographicHash::Sha256);
    if (!hash.addData(&file) || hash.result().toHex() != package.digest)
        return application::InstallResult::Failed;
    return QProcess::startDetached(QStringLiteral("xdg-open"), {package.path})
               ? application::InstallResult::Started
               : application::InstallResult::Failed;
}
} // namespace manager::adapters
