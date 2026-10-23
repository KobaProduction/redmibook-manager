#include "apps/tray/device_controls.hpp"
#include "adapters/platform/firmware_controls.hpp"
#include "adapters/platform/power_controls.hpp"
#include "adapters/platform/power_scheme.hpp"
#include "adapters/platform/session_controls.hpp"
#include "application/display/cycle_refresh.hpp"
#include <QtConcurrentRun>
#include <array>
#include <utility>

namespace manager::tray {
namespace {
constexpr int operation_timeout_ms = 10000;
constexpr std::array profiles{application::performance::Profile::Quiet,
                              application::performance::Profile::Balanced,
                              application::performance::Profile::Turbo};
bool observed(application::performance::Status status) {
    return status == application::performance::Status::Available ||
           status == application::performance::Status::Confirmed;
}
} // namespace

DeviceControls::DeviceControls(presentation::TrayClient &tray, presentation::Preferences &preferences)
    : tray_(tray), preferences_(preferences), performance_(adapters::make_core_performance_control()),
      refresh_(std::make_shared<adapters::NativeRefresh>()),
      preset_executor_(std::make_shared<adapters::PresetExecutor>(
          performance_, refresh_, adapters::make_power_schemes(), adapters::make_brightness(),
          adapters::make_energy_saver())) {
    deadline_.setSingleShot(true);
    wire_state();
    connect(&tray_, &presentation::TrayClient::profileRequested, this, &DeviceControls::set_profile);
    connect(&tray_, &presentation::TrayClient::refreshRequested, this, &DeviceControls::cycle_refresh);
    connect(&tray_, &presentation::TrayClient::presetRequested, this, &DeviceControls::apply_preset);
    watch_visibility();
    connect(&preferences_, &presentation::Preferences::changed, this, &DeviceControls::project);
    connect(&preferences_, &presentation::Preferences::languageChanged, this, &DeviceControls::project);
    connect(&watcher_, &QFutureWatcher<bool>::finished, this, &DeviceControls::complete);
    connect(&deadline_, &QTimer::timeout, this, [this] {
        if (busy_) {
            feedback_ = Feedback::TimedOut;
            project();
            state_client_.refresh();
        }
    });
}

void DeviceControls::wire_state() {
    connect(&state_client_, &adapters::DeviceStateClient::stateChanged, this,
            [this](const application::overview::DeviceState &state) {
                state_ = state;
                connected_ = true;
                project();
            });
    connect(&state_client_, &adapters::DeviceStateClient::unavailable, this, [this] {
        connected_ = false;
        project();
    });
}

void DeviceControls::watch_visibility() {
    battery_lease_.setInterval(1000);
    connect(&battery_lease_, &QTimer::timeout, this, [this] { state_client_.watchBattery(1000); });
    connect(&tray_, &presentation::TrayClient::dashboardVisibilityChanged, this, [this](bool visible) {
        if (visible) {
            state_client_.watchBattery(1000);
            state_client_.refreshBattery();
            battery_lease_.start();
        } else {
            battery_lease_.stop();
            state_client_.watchBattery(0);
        }
    });
}

void DeviceControls::start() {
    state_client_.start();
}

namespace {
int observed_profile(const application::performance::Observation &value) {
    if (!observed(value.status) || !value.profile)
        return -1;
    for (std::size_t index = 0; index < profiles.size(); ++index)
        if (*value.profile == profiles[index])
            return static_cast<int>(index);
    return -1;
}
void project_display(const application::display::Snapshot &displays, const std::vector<unsigned int> &rates,
                     presentation::TrayDeviceState &view) {
    const auto *panel = application::display::unique_internal_panel(displays);
    view.refreshSupported = displays.status == application::display::Status::Ready && panel;
    if (!view.refreshSupported)
        return;
    view.refreshHz = static_cast<int>(panel->current_hz);
    view.refreshRates = application::display::selected_refresh_rates(panel->available_hz, rates);
    view.refreshAvailable = !view.refreshRates.empty();
}
presentation::TrayDeviceState project_state(const application::overview::DeviceState &state,
                                            const std::vector<unsigned int> &rates) {
    presentation::TrayDeviceState view;
    view.profileIndex = observed_profile(state.performance);
    view.profileAvailable =
        view.profileIndex >= 0 && state.performance_capability == application::performance::Status::Available;
    project_display(state.displays, rates, view);
    view.microphoneAvailable = state.microphone.status == application::microphone::Status::Ready &&
                               state.microphone.muted.has_value();
    view.microphoneMuted = state.microphone.muted.value_or(false);
    if (state.battery.limit && state.battery.status == application::battery::Status::Available)
        view.chargeLimit = static_cast<int>(*state.battery.limit);
    if (state.battery_telemetry.status == application::battery::TelemetryStatus::Available) {
        view.batteryRateW = state.battery_telemetry.rate_w;
        view.batteryDischarging = state.battery_telemetry.discharging.value_or(false);
    }
    view.profileSupported = state.performance_capability == application::performance::Status::Available;
    return view;
}
} // namespace

void DeviceControls::preserve_support(presentation::TrayDeviceState &view) {
    profile_supported_ = profile_supported_ || view.profileSupported;
    refresh_supported_ = refresh_supported_ || view.refreshSupported;
    view.profileSupported = profile_supported_;
    view.refreshSupported = refresh_supported_;
}
void DeviceControls::project() {
    auto view = connected_ ? project_state(state_, preferences_.refresh_cycle_rates())
                           : presentation::TrayDeviceState{};
    preserve_support(view);
    view.pending = busy_;
    view.sections = preferences_.tray_sections();
    view.presets = preferences_.tray_presets();
    view.error = feedback_ == Feedback::TimedOut || feedback_ == Feedback::Unconfirmed;
    if (feedback_ == Feedback::TimedOut)
        view.status = tr("The operation timed out; its result is not confirmed.");
    else if (feedback_ == Feedback::Unconfirmed)
        view.status =
            pending_preset_ && preset_result_
                ? (preset_result_->unavailable
                       ? tr("A preset action is unavailable. No changes were made.")
                       : tr("Preset stopped. Confirmed actions: %1.").arg(preset_result_->completed))
                : tr("The change could not be confirmed.");
    else if (!connected_)
        view.status = tr("Background device state unavailable.");
    tray_.setDeviceState(std::move(view));
}

void DeviceControls::set_profile(int index) {
    if (busy_ || !connected_ || index < 0 || index >= static_cast<int>(profiles.size()) ||
        state_.performance_capability != application::performance::Status::Available)
        return;
    const auto profile = profiles[static_cast<std::size_t>(index)];
    run(
        [control = performance_, profile] {
            return application::performance::apply_profile(*control, profile).status ==
                   application::performance::Status::Confirmed;
        },
        {application::feedback::Kind::Performance, static_cast<unsigned int>(profile)});
}

void DeviceControls::cycle_refresh(unsigned int hz) {
    if (busy_ || !connected_ || state_.displays.status != application::display::Status::Ready)
        return;
    run(
        [control = refresh_, rates = preferences_.refresh_cycle_rates(), hz] {
            const auto snapshot = control->read();
            const auto *panel = application::display::unique_internal_panel(snapshot);
            if (snapshot.status != application::display::Status::Ready || !panel)
                return false;
            const auto selected = application::display::selected_refresh_rates(panel->available_hz, rates);
            if (std::ranges::find(selected, hz) == selected.end())
                return false;
            const auto result = control->set_refresh(panel->id, hz);
            return result.status == application::display::Status::Ready && result.observed_hz == hz;
        },
        {application::feedback::Kind::Refresh, hz});
}

void DeviceControls::run(std::function<bool()> operation, application::feedback::Event feedback,
                         bool preset) {
    pending_preset_ = preset;
    pending_feedback_ = feedback;
    busy_ = true;
    feedback_ = Feedback::Applying;
    elapsed_.start();
    deadline_.start(operation_timeout_ms);
    project();
    watcher_.setFuture(QtConcurrent::run([operation = std::move(operation)] {
        try {
            return operation();
        } catch (...) {
            return false;
        }
    }));
}

void DeviceControls::complete() {
    deadline_.stop();
    // A GUI deadline never cancels a native operation. Keep singleflight until it returns.
    const bool expired = feedback_ == Feedback::TimedOut || elapsed_.elapsed() >= operation_timeout_ms;
    const bool confirmed = watcher_.result();
    busy_ = false;
    feedback_ = expired ? Feedback::TimedOut : (confirmed ? Feedback::None : Feedback::Unconfirmed);
    if (!expired && confirmed && !pending_preset_)
        (void)feedback_client_.present(pending_feedback_);
    project();
    state_client_.refresh();
}
} // namespace manager::tray
