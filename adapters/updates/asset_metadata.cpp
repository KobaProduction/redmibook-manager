#include "adapters/updates/release_feed.hpp"
#include <QJsonObject>

namespace manager::adapters {
application::UpdateResult release_package(application::UpdateResult result, const QJsonObject &object) {
    for (const auto &entry : object.value(QStringLiteral("assets")).toArray()) {
        const auto asset = entry.toObject();
        if (asset.value(QStringLiteral("browser_download_url")).toString().toStdString() !=
            result.download_url)
            continue;
        const auto size = asset.value(QStringLiteral("size")).toInteger();
        const auto digest = asset.value(QStringLiteral("digest")).toString();
        if (size > 0 && digest.startsWith(QStringLiteral("sha256:"))) {
            result.download_size = static_cast<std::uint64_t>(size);
            result.download_sha256 = digest.mid(7).toStdString();
        }
        break;
    }
    return result;
}
} // namespace manager::adapters
