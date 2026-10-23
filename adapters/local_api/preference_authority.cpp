#include "adapters/local_api/preference_authority.hpp"
#include <QDir>
#include <QtConcurrentRun>

namespace manager::adapters {
PreferenceAuthority::PreferenceAuthority(QString directory, QString endpoint,
                                         std::function<application::presets::Catalog()> starter_catalog)
    : directory_(std::move(directory)), endpoint_(std::move(endpoint)),
      ownership_(QDir(directory_).filePath(QStringLiteral("preferences-authority.lock"))), client_(endpoint_),
      starter_catalog_(std::move(starter_catalog)) {
    connect(&starter_watcher_, &QFutureWatcher<application::presets::Catalog>::finished, this, [this] {
        try {
            if (store_ && !store_->configure_starter_defaults(starter_watcher_.result()))
                qWarning("starter-preset-save-failed");
        } catch (...) {
            qWarning("starter-preset-catalog-failed");
        }
    });
    ownership_.setStaleLockTime(0);
    QDir().mkpath(directory_);
    takeover_.setSingleShot(true);
    connect(&takeover_, &QTimer::timeout, this, &PreferenceAuthority::claim);
    client_.on_connection_failure([this] {
        if (!server_ && !takeover_.isActive())
            takeover_.start(0);
    });
    claim();
}
void PreferenceAuthority::claim() {
    if (server_ || !ownership_.tryLock())
        return;
    store_ = std::make_unique<UiPreferenceStore>(directory_, true);
    server_ = std::make_unique<PreferenceServer>(*store_, endpoint_);
    if (!server_->listen()) {
        server_.reset();
        store_.reset();
        ownership_.unlock();
        return;
    }
    store_->observe([this] { server_->broadcast(); });
    if (starter_catalog_)
        starter_watcher_.setFuture(QtConcurrent::run(starter_catalog_));
    client_.reconnect();
}
PreferenceAuthority::~PreferenceAuthority() {
    client_.on_connection_failure({});
    takeover_.stop();
    // Stop commands, then drain persistence before releasing the role lease.
    // A successor cannot acquire ownership/load disk until that lease is freed.
    server_.reset();
    store_.reset();
    ownership_.unlock();
}
} // namespace manager::adapters
