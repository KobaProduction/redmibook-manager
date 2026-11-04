#include "adapters/updates/github_client.hpp"
#include "adapters/updates/history.hpp"
#include "adapters/updates/package_client.hpp"
#include "adapters/updates/release_feed.hpp"
#include "application/updates/release_identity.hpp"
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTextStream>
#include <QTimer>
namespace {
using manager::application::UpdateState;
QJsonObject release(const QString &tag, bool alpha = false) {
    const QString asset =
        QStringLiteral("https://github.com/KobaProduction/redmibook-manager/releases/download/") + tag +
        QStringLiteral("/RedmibookManager-0.1.0-Windows-x86_64.exe");
    return {{QStringLiteral("tag_name"), tag},
            {QStringLiteral("draft"), false},
            {QStringLiteral("prerelease"), alpha},
            {QStringLiteral("html_url"),
             QStringLiteral("https://github.com/KobaProduction/redmibook-manager/releases/tag/") + tag},
            {QStringLiteral("assets"),
             QJsonArray{QJsonObject{
                 {QStringLiteral("name"), QStringLiteral("RedmibookManager-0.1.0-Windows-x86_64.exe")},
                 {QStringLiteral("browser_download_url"), asset}}}}};
}
bool identities() {
    using manager::application::release_identity;
    const auto alpha = release_identity("v0.1.0-alpha.2");
    const auto alpha10 = release_identity("0.1.0-alpha.10");
    const auto stable = release_identity("0.1.0");
    return alpha && alpha10 && stable && *alpha < *stable && *alpha10 > *alpha && !release_identity("0.1") &&
           !release_identity("0.1.0-alpha.-1") && !release_identity("0.1.0.4") &&
           !release_identity("0.1.0-beta.1") && !release_identity("0.1.0-alpha.1junk");
}
bool asset_selection() {
    using manager::adapters::decode_release;
    const auto data = release(QStringLiteral("v0.1.0"));
    const auto windows = decode_release(data, QStringLiteral("windows-x86_64"));
    const auto arm = decode_release(data, QStringLiteral("windows-arm64"));
    // This fixture is older than the installed product; asset selection is still valid.
    return windows.state == UpdateState::Current && !windows.download_url.empty() &&
           arm.download_url.empty() && !arm.release_url.empty() &&
           manager::adapters::decode_releases({release(QStringLiteral("v0.1.0")), data},
                                              QStringLiteral("windows-x86_64"))
                   .version == "0.1.0" &&
           manager::adapters::decode_releases({}, {}).state == UpdateState::Unavailable;
}

bool renamed_installer_selection() {
    auto data = release(QStringLiteral("v0.1.0"));
    auto assets = data.value(QStringLiteral("assets")).toArray();
    auto installer = assets.first().toObject();
    const auto name = QStringLiteral("RedmibookManager-0.1.0-x86_64.exe");
    const auto url =
        QStringLiteral("https://github.com/KobaProduction/redmibook-manager/releases/download/v0.1.0/") +
        name;
    installer.insert(QStringLiteral("name"), name);
    installer.insert(QStringLiteral("browser_download_url"), url);
    assets.append(installer);
    data.insert(QStringLiteral("assets"), assets);
    const auto preferred = manager::adapters::decode_release(data, QStringLiteral("windows-x86_64"));
    data.insert(QStringLiteral("assets"), QJsonArray{installer});
    const auto only = manager::adapters::decode_release(data, QStringLiteral("windows-x86_64"));
    return preferred.download_url == url.toStdString() && only.download_url == preferred.download_url;
}
bool url_validation() {
    using manager::adapters::trusted_release_url;
    return !trusted_release_url(QUrl(QStringLiteral("https://evil.example/releases/"))) &&
           !trusted_release_url(QUrl(QStringLiteral("https://github.com/elsewhere/repo/releases/"))) &&
           !trusted_release_url(QUrl(QStringLiteral(
               "https://user@github.com/KobaProduction/redmibook-manager/releases/tag/v1.0.0"))) &&
           !trusted_release_url(QUrl(
               QStringLiteral("https://github.com/KobaProduction/redmibook-manager/releases/../issues/1"))) &&
           !trusted_release_url(QUrl(
               QStringLiteral("http://github.com/KobaProduction/redmibook-manager/releases/tag/v1.0.0")));
}
bool history_contract() {
    QTemporaryDir directory;
    manager::adapters::UpdateHistory history(directory.path());
    if (!history.claim_daily_check() || history.claim_daily_check()) {
        return false;
    }
    QFile file(QDir(directory.path()).filePath(QStringLiteral("updates.last-check")));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }
    file.write(QByteArray::number(QDateTime::currentSecsSinceEpoch() - 86401));
    file.close();
    return history.claim_daily_check() && !history.claim_daily_check();
}
void verify_download(QCoreApplication &app, manager::adapters::UpdatePackages &packages,
                     manager::application::UpdateResult release) {
    release.state =
        UpdateState::Available; // Read-only integration probe can fetch the installed release too.
    packages.download(
        release, [](std::uint64_t, std::uint64_t) {},
        [&app](bool verified) {
            QTextStream(stdout) << "Downloaded and verified: " << verified << Qt::endl;
            app.exit(verified ? 0 : 1);
        });
}
int network_probe(QCoreApplication &app) {
    QTemporaryDir directory;
    manager::adapters::UpdatePackages packages(directory.path());
    manager::adapters::GitHubUpdates source(QStringLiteral("windows-x86_64"));
    source.check([&app, &packages](const manager::application::UpdateResult &result) {
        QTextStream(stdout) << "Update state: " << static_cast<int>(result.state)
                            << " version: " << QString::fromStdString(result.version) << Qt::endl;
        if (app.arguments().contains(QStringLiteral("--download")) && result.state != UpdateState::Failed) {
            verify_download(app, packages, result);
            return;
        }
        app.exit(result.state == UpdateState::Failed ? 1 : 0);
    });
    QTimer::singleShot(app.arguments().contains(QStringLiteral("--download")) ? 120000 : 12000, &app,
                       [] { QCoreApplication::exit(2); });
    return app.exec();
}
} // namespace
int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);
    if (app.arguments().contains(QStringLiteral("--network"))) {
        return network_probe(app);
    }
    return identities() && asset_selection() && renamed_installer_selection() && url_validation() &&
                   history_contract()
               ? 0
               : 1;
}
