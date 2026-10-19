#pragma once
#include "adapters/device_profile/demo_profile.hpp"
#include "adapters/local_api/session_client.hpp"
#include "adapters/storage/ui_preferences/store.hpp"
#include "adapters/updates/github_client.hpp"
#include "apps/desktop/manager_window.hpp"
#include "presentation/about_model.hpp"
#include "presentation/common/preferences.hpp"
#include "presentation/manager_model.hpp"
#include "presentation/updates/update_model.hpp"
#include <QQmlApplicationEngine>
#include <QTemporaryDir>

namespace manager::desktop {
class ManagerHost final {
  public:
    explicit ManagerHost(bool smoke);
    void setRestored(bool restored) {
        restored_ = restored;
    }
    int open(bool smoke, const QString &screenshot, bool ui_contract = false);

  private:
    QTemporaryDir temporary_;
    adapters::UiPreferenceStore store_;
    presentation::Preferences preferences_;
    adapters::SessionClient client_;
    presentation::ManagerModel model_;
    adapters::DemoDeviceProfile profile_source_;
    presentation::AboutModel about_;
    adapters::GitHubUpdates update_source_;
    presentation::UpdateModel updates_;
    QQmlApplicationEngine engine_;
    ManagerInstance instance_;
    bool restored_{false};
};
} // namespace manager::desktop
