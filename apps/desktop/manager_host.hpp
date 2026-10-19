#pragma once
#include "adapters/device_profile/native_profile.hpp"
#include "adapters/local_api/device_state_stream.hpp"
#include "adapters/local_api/session_client.hpp"
#include "adapters/platform/session_controls.hpp"
#include "adapters/storage/ui_preferences/store.hpp"
#include "adapters/updates/github_client.hpp"
#include "apps/desktop/manager_window.hpp"
#include "presentation/about_model.hpp"
#include "presentation/common/preferences.hpp"
#include "presentation/device_controls_model.hpp"
#include "presentation/manager_model.hpp"
#include "presentation/updates/update_model.hpp"
#include <QQmlApplicationEngine>
#include <QTemporaryDir>
#include <memory>

namespace manager::desktop {
class ManagerHost final {
  public:
    explicit ManagerHost(bool smoke);
    void setRestored(bool restored) {
        restored_ = restored;
    }
    int open(bool smoke, const QString &screenshot, bool ui_contract = false,
             ManagerActivation activation = ManagerActivation::KeepCurrent);

  private:
    QTemporaryDir temporary_;
    adapters::UiPreferenceStore store_;
    presentation::Preferences preferences_;
    adapters::SessionClient client_;
    presentation::ManagerModel model_;
    adapters::NativeDeviceProfile profile_source_;
    presentation::AboutModel about_;
    std::shared_ptr<adapters::NativeMicrophone> microphone_{std::make_shared<adapters::NativeMicrophone>()};
    std::shared_ptr<adapters::NativeRefresh> refresh_{std::make_shared<adapters::NativeRefresh>()};
    presentation::DeviceControlsModel controls_;
    adapters::DeviceStateClient state_client_;
    adapters::GitHubUpdates update_source_;
    presentation::UpdateModel updates_;
    QQmlApplicationEngine engine_;
    ManagerInstance instance_;
    bool restored_{false};
    void wire_device_state(bool smoke);
};
} // namespace manager::desktop
