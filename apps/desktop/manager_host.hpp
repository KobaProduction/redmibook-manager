#pragma once
#include "adapters/device_profile/native_profile.hpp"
#include "adapters/integration/preset_executor.hpp"
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
#include "presentation/power_controls_model.hpp"
#include "presentation/presets/presets_model.hpp"
#include "presentation/updates/update_model.hpp"
#include <QQmlApplicationEngine>
#include <QTemporaryDir>
#include <memory>

namespace manager::desktop {
class ManagerHost final {
  public:
    explicit ManagerHost(bool smoke, std::unique_ptr<application::PreferencePort> store = {});
    void setRestored(bool restored) {
        restored_ = restored;
    }
    int open(bool smoke, const QString &screenshot, bool ui_contract = false,
             ManagerActivation activation = ManagerActivation::KeepCurrent, bool preset_preview = false);

  private:
    QTemporaryDir temporary_;
    std::unique_ptr<application::PreferencePort> store_;
    presentation::Preferences preferences_;
    adapters::SessionClient client_;
    presentation::ManagerModel model_;
    adapters::NativeDeviceProfile profile_source_;
    presentation::AboutModel about_;
    std::shared_ptr<adapters::NativeMicrophone> microphone_{std::make_shared<adapters::NativeMicrophone>()};
    std::shared_ptr<adapters::NativeRefresh> refresh_{std::make_shared<adapters::NativeRefresh>()};
    presentation::DeviceControlsModel controls_;
    std::shared_ptr<application::display::BrightnessPort> brightness_;
    std::shared_ptr<application::power::SaverPort> saver_;
    presentation::PowerControlsModel power_controls_;
    std::shared_ptr<adapters::PresetExecutor> preset_executor_;
    presentation::PresetsModel presets_;
    adapters::DeviceStateClient state_client_;
    adapters::GitHubUpdates update_source_;
    presentation::UpdateModel updates_;
    QQmlApplicationEngine engine_;
    ManagerInstance instance_;
    bool restored_{false};
    bool content_started_{false};
    void load_content();
    void present(bool smoke, ManagerActivation activation);
    void present_when_ready(ManagerActivation activation);
    void wire_preferences(bool smoke);
    void schedule_preview(bool smoke, const QString &screenshot, bool ui_contract, bool preset_preview);
    void wire_device_state(bool smoke);
};
} // namespace manager::desktop
