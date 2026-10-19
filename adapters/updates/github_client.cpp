#include "adapters/updates/github_client.hpp"
#include "adapters/updates/release_feed.hpp"
#include "application/updates/release_identity.hpp"
#include "application/updates/release_policy.hpp"
#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkReply>
#include <QTimer>

namespace manager::adapters {
bool trusted_release_url(const QUrl &url) {
    return url.scheme() == QStringLiteral("https") && url.host() == QStringLiteral("github.com") &&
           url.userInfo().isEmpty() && url.port(-1) == -1 && url.query().isEmpty() &&
           url.fragment().isEmpty() && url.adjusted(QUrl::NormalizePathSegments).path() == url.path() &&
           url.path().startsWith(QStringLiteral("/KobaProduction/redmibook-manager/releases/"));
}
application::UpdateResult decode_release(const QJsonObject &object, const QString &target) {
    const auto tag = object.value(QStringLiteral("tag_name")).toString();
    const auto identity = application::release_identity(tag.toStdString());
    const QUrl page(object.value(QStringLiteral("html_url")).toString());
    const bool prerelease = object.value(QStringLiteral("prerelease")).toBool();
    if (!identity || !trusted_release_url(page) || object.value(QStringLiteral("draft")).toBool() ||
        prerelease == identity->stable ||
        (std::string_view(MANAGER_RELEASE_CHANNEL) == "stable" && prerelease)) {
        return {.state = application::UpdateState::Failed};
    }
    const auto &version = identity->version;
    const auto canonical = QStringLiteral("%1.%2.%3").arg(version[0]).arg(version[1]).arg(version[2]);
    return {.state = *identity > application::installed_release() ? application::UpdateState::Available
                                                                  : application::UpdateState::Current,
            .version = (identity->stable ? canonical
                                         : canonical + QStringLiteral("-alpha.%1").arg(identity->sequence))
                           .toStdString(),
            .release_url = page.toString().toStdString(),
            .download_url = release_asset(object, version, target).toStdString()};
}
GitHubUpdates::GitHubUpdates(QString target) : target_(std::move(target)) {}
GitHubUpdates::~GitHubUpdates() {
    for (auto *reply : network_.findChildren<QNetworkReply *>()) {
        reply->disconnect(this);
        reply->abort();
    }
}
namespace {
application::UpdateResult reply_result(QNetworkReply &reply, const QString &target) {
    if (reply.attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 404) {
        return {.state = application::UpdateState::Unavailable};
    }
    if (reply.error() == QNetworkReply::NoError && reply.bytesAvailable() <= 1048576) {
        const auto document = QJsonDocument::fromJson(reply.readAll());
        if (document.isArray()) {
            return decode_releases(document.array(), target);
        }
    }
    return {.state = application::UpdateState::Failed};
}
void limit_reply(QNetworkReply *reply) {
    QTimer::singleShot(10000, reply, [reply] {
        if (reply->isRunning()) {
            reply->abort();
        }
    });
    QObject::connect(reply, &QNetworkReply::readyRead, reply, [reply] {
        if (reply->bytesAvailable() > 1048576) {
            reply->abort();
        }
    });
}
} // namespace

void GitHubUpdates::check(std::function<void(application::UpdateResult)> completion) {
    QNetworkRequest request(QUrl(QStringLiteral(
        "https://api.github.com/repos/KobaProduction/redmibook-manager/releases?per_page=100")));
    request.setRawHeader("User-Agent", "RedmiBookManager/" MANAGER_APP_VERSION);
    request.setRawHeader("Accept", "application/vnd.github+json");
    request.setTransferTimeout(10000);
    auto *reply = network_.get(request);
    limit_reply(reply);
    connect(reply, &QNetworkReply::finished, this, [this, reply, completion = std::move(completion)] {
        const auto result = reply_result(*reply, target_);
        reply->deleteLater();
        completion(result);
    });
}
} // namespace manager::adapters
