#pragma once
#include "adapters/local_api/preference_channel.hpp"
#include <QLockFile>
#include <functional>
#include <memory>

namespace manager::adapters {
// Each session hosts its own input/OSD runtime. Exactly one of them owns the
// shared per-user preference role; all sessions consume its same IPC feed.
class PreferenceAuthority final : public QObject {
  public:
    PreferenceAuthority(QString directory, QString endpoint,
                        std::function<application::presets::Catalog()> starter_catalog = {});
    ~PreferenceAuthority() override;
    PreferenceClient &client() {
        return client_;
    }
    bool owns_authority() const {
        return server_ != nullptr;
    }

  private:
    QString directory_;
    QString endpoint_;
    QLockFile ownership_;
    std::unique_ptr<UiPreferenceStore> store_;
    std::unique_ptr<PreferenceServer> server_;
    PreferenceClient client_;
    QTimer takeover_;
    std::function<application::presets::Catalog()> starter_catalog_;
    QFutureWatcher<application::presets::Catalog> starter_watcher_;
    void claim();
};
} // namespace manager::adapters
