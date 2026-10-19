#include "apps/tray/device_controls.hpp"
#include "adapters/platform/firmware_controls.hpp"
#include "adapters/platform/session_controls.hpp"
#include "application/display/cycle_refresh.hpp"
#include <QtConcurrentRun>
#include <array>
#include <utility>

namespace manager::tray {
namespace {
constexpr int operation_timeout_ms = 10000;
constexpr std::array profiles{application::performance::Profile::Balanced,
                              application::performance::Profile::Quiet,
                              application::performance::Profile::Turbo};
bool observed(application::performance::Status status) {
    return status == application::performance::Status::Available ||
           status == application::performance::Status::Confirmed;
}
} // namespace

DeviceControls::DeviceControls(presentation::TrayClient &tray, presentation::Preferences &preferences)
    : tray_(tray), preferences_(preferences), performance_(adapters::make_core_performance_control()),
      refresh_(std::make_shared<adapters::NativeRefresh>()) {
    deadline_.setSingleShot(true);
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
    connect(&tray_, &presentation::TrayClient::profileRequested, this, &DeviceControls::set_profile);
    connect(&tray_, &presentation::TrayClient::refreshRequested, this, &DeviceControls::cycle_refresh);
    connect(&tray_, &presentation::TrayClient::dashboardOpened, &state_client_,
            &adapters::DeviceStateClient::refresh);
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
presentation::TrayDeviceState project_state(const application::overview::DeviceState &state,
                                            const std::vector<unsigned int> &rates) {
    presentation::TrayDeviceState view;
    view.profileIndex = observed_profile(state.performance);
    view.profileAvailable =
        view.profileIndex >= 0 && state.performance_capability == application::performance::Status::Available;
    const auto *panel = application::display::unique_internal_panel(state.displays);
    if (state.displays.status == application::display::Status::Ready && panel) {
        view.refreshHz = static_cast<int>(panel->current_hz);
        view.refreshAvailable =
            !application::display::selected_refresh_rates(panel->available_hz, rates).empty();
    }
    view.microphoneAvailable = state.microphone.status == application::microphone::Status::Ready &&
                               state.microphone.muted.has_value();
    view.microphoneMuted = state.microphone.muted.value_or(false);
    return view;
}
} // namespace

void DeviceControls::project() {
    auto view = connected_ ? project_state(state_, preferences_.refresh_cycle_rates())
                           : presentation::TrayDeviceState{};
    view.pending = busy_;
    if (feedback_ == Feedback::TimedOut)
        view.status = tr("The operation timed out; its result is not confirmed.");
    else if (feedback_ == Feedback::Unconfirmed)
        view.status = tr("The change could not be confirmed.");
    else if (busy_)
        view.status = tr("Applying change…");
    else if (!connected_)
        view.status = tr("Background device state unavailable.");
    tray_.setDeviceState(std::move(view));
}

void DeviceControls::set_profile(int index) {
    if (busy_ || !connected_ || index < 0 || index >= static_cast<int>(profiles.size()) ||
        state_.performance_capability != application::performance::Status::Available)
        return;
    const auto profile = profiles[static_cast<std::size_t>(index)];
    run([control = performance_, profile] {
        return application::performance::apply_profile(*control, profile).status ==
               application::performance::Status::Confirmed;
    });
}

void DeviceControls::cycle_refresh() {
    if (busy_ || !connected_ || state_.displays.status != application::display::Status::Ready)
        return;
    run([control = refresh_, rates = preferences_.refresh_cycle_rates()] {
        return application::display::cycle_internal_refresh(*control, rates).status ==
               application::display::Status::Ready;
    });
}

void DeviceControls::run(std::function<bool()> operation) {
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
    project();
    state_client_.refresh();
}
} // namespace manager::tray
