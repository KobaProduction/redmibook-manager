#include "adapters/updates/history.hpp"
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QLockFile>
#include <QSaveFile>

namespace manager::adapters {
UpdateHistory::UpdateHistory(QString directory) : directory_(std::move(directory)) {}
bool UpdateHistory::claim_daily_check() {
    const auto path = QDir(directory_).filePath(QStringLiteral("updates.last-check"));
    QLockFile lock(path + QStringLiteral(".lock"));
    if (!lock.tryLock(0)) {
        return false;
    }
    QFile input(path);
    const auto now = QDateTime::currentSecsSinceEpoch();
    if (input.open(QIODevice::ReadOnly)) {
        const auto previous = input.readAll().toLongLong();
        input.close();
        if (previous <= now && now - previous < 86400) {
            return false;
        }
    }
    QSaveFile output(path);
    return output.open(QIODevice::WriteOnly) && output.write(QByteArray::number(now)) != -1 &&
           output.commit();
}
} // namespace manager::adapters
