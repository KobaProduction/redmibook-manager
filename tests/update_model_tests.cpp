#include "presentation/updates/update_model.hpp"
#include <QApplication>
#include <QCoreApplication>
#include <memory>
int run_update_preview(QApplication &app, const QString &directory);

namespace {
namespace app = manager::application;
class Feed final : public app::UpdatePort {
  public:
    void check(std::function<void(app::UpdateResult)> completion) override {
        completion(
            {.state = app::UpdateState::Available,
             .version = "999.0.0",
             .release_url = "https://github.com/KobaProduction/redmibook-manager/releases/tag/v999.0.0",
             .download_url = "https://github.com/KobaProduction/redmibook-manager/releases/download/v999.0.0/"
                             "RedmibookManager-999.0.0-x86_64.exe",
             .download_sha256 = std::string(64, 'a'),
             .download_size = 100});
    }
};
class Packages final : public app::UpdatePackagePort {
  public:
    int downloads{};
    int installs{};
    Progress progress;
    std::function<void(bool)> downloaded;
    std::function<void(app::InstallResult)> installed;
    void download(const app::UpdateResult &, Progress update, std::function<void(bool)> complete) override {
        ++downloads;
        progress = std::move(update);
        downloaded = std::move(complete);
    }
    void install(std::function<void(app::InstallResult)> complete) override {
        ++installs;
        installed = std::move(complete);
    }
};
bool installation(manager::presentation::UpdateModel &model, Packages &packages) {
    packages.downloaded(true);
    model.install();
    model.install();
    if (!model.busy() || packages.installs != 1)
        return false;
    packages.installed(app::InstallResult::Cancelled);
    if (model.busy() || !model.canInstall())
        return false;
    model.install();
    packages.installed(app::InstallResult::Started);
    return !model.busy() && !model.canInstall() && !model.canDownload();
}
bool lifecycle() {
    Feed feed;
    Packages packages;
    manager::presentation::UpdateModel model(feed, QStringLiteral("windows-x86_64"), &packages);
    model.check();
    if (!model.canDownload() || model.canInstall())
        return false;
    model.download();
    model.download();
    model.install();
    if (!model.busy() || packages.downloads != 1 || packages.installs != 0)
        return false;
    packages.progress(50, 100);
    return model.progress() == 0.5 && installation(model, packages);
}
bool failure_retry(manager::presentation::UpdateModel &model, Packages &packages) {
    model.check();
    model.download();
    packages.downloaded(false);
    if (model.canInstall() || !model.canDownload() || model.busy())
        return false;
    model.download();
    packages.downloaded(true);
    model.install();
    packages.installed(app::InstallResult::Failed);
    return !model.canInstall() && model.canDownload();
}
bool failures_and_lifetime() {
    Feed feed;
    Packages packages;
    auto model = std::make_unique<manager::presentation::UpdateModel>(feed, QStringLiteral("windows-x86_64"),
                                                                      &packages);
    if (!failure_retry(*model, packages))
        return false;
    model->download();
    model.reset();
    packages.progress(100, 100);
    packages.downloaded(true);
    return true;
}
} // namespace
int main(int argc, char *argv[]) {
    if (argc == 3 && std::string_view(argv[1]) == "--preview") {
        QApplication app(argc, argv);
        return run_update_preview(app, QString::fromLocal8Bit(argv[2]));
    }
    QCoreApplication application(argc, argv);
    return lifecycle() && failures_and_lifetime() ? 0 : 1;
}
