#include "adapters/updates/release_feed.hpp"
#include "adapters/updates/github_client.hpp"
#include "application/updates/release_identity.hpp"
namespace manager::adapters {
QString release_asset(const QJsonObject &object, const application::ReleaseVersion &version,
                      const QString &target) {
    const auto name = QString::fromStdString(application::package_asset(version, target.toStdString()));
    const auto legacy =
        target == QStringLiteral("windows-x86_64")
            ? QString(name).replace(QStringLiteral("-x86_64.exe"), QStringLiteral("-Windows-x86_64.exe"))
            : QString{};
    QString fallback;
    for (const auto &asset : object.value(QStringLiteral("assets")).toArray()) {
        const auto metadata = asset.toObject();
        const QUrl url(metadata.value(QStringLiteral("browser_download_url")).toString());
        if (!trusted_release_url(url)) {
            continue;
        }
        const auto candidate = metadata.value(QStringLiteral("name")).toString();
        if (!name.isEmpty() && candidate == name) {
            return url.toString();
        }
        if (!legacy.isEmpty() && candidate == legacy) {
            fallback = url.toString();
        }
    }
    return fallback;
}
application::UpdateResult decode_releases(const QJsonArray &releases, const QString &target) {
    application::UpdateResult result;
    std::optional<application::ReleaseIdentity> latest;
    for (const auto &entry : releases) {
        const auto release = decode_release(entry.toObject(), target);
        const auto identity = application::release_identity(release.version);
        if (identity && (!latest || *identity > *latest)) {
            latest = identity;
            result = release;
        }
    }
    return result;
}
} // namespace manager::adapters
