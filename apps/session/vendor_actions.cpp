#include "apps/session/vendor_actions.hpp"
#include "adapters/device_profile/native_profile.hpp"
#include "adapters/platform/firmware_controls.hpp"
#include "adapters/platform/vendor_input.hpp"
#include "application/display/cycle_refresh.hpp"
#include "application/microphone/synchronize.hpp"
#include <QDebug>
#include <QtConcurrent/QtConcurrentRun>

namespace manager::session {
namespace shortcuts = application::shortcuts;
VendorActions::VendorActions(presentation::Preferences &preferences, presentation::OsdPresenter &osd,
                             WorkQueues &work, std::function<void()> open_manager,
                             std::function<void()> open_tray, std::function<void()> state_changed)
    : work_(work), preferences_(preferences), delivery_(std::make_shared<Delivery>()),
      microphone_(std::make_shared<adapters::NativeMicrophone>()),
      microphone_firmware_(adapters::make_core_microphone_state()),
      display_(std::make_shared<adapters::NativeRefresh>()), osd_(osd),
      open_manager_(std::move(open_manager)), open_tray_(std::move(open_tray)),
      state_changed_(std::move(state_changed)) {
    delivery_->receiver = this;
    configure_microphone();
    configure_display();
    connect(&profile_result_, &QFutureWatcher<application::DeviceProfile>::finished, this,
            &VendorActions::profile_ready);
    profile_result_.setFuture(
        QtConcurrent::run(&work_.observations, [] { return adapters::NativeDeviceProfile{}.read(); }));
}
void VendorActions::configure_microphone() {
    microphone_deadline_.setSingleShot(true);
    connect(&microphone_deadline_, &QTimer::timeout, this, [this] {
        if (!microphone_background_)
            osd_.present({application::microphone::Status::Failed, {}, "microphone-timeout"});
    });
    connect(&microphone_result_, &QFutureWatcher<application::microphone::Observation>::finished, this,
            &VendorActions::complete_microphone);
    configure_initial_microphone();
}
void VendorActions::configure_initial_microphone() {
    const auto delivery = delivery_;
    initial_microphone_ = std::make_unique<InitialMicrophoneRead>(
        work_, microphone_firmware_,
        [delivery] {
            const std::lock_guard lock(delivery->mutex);
            return delivery->microphone_generation != 0 || delivery->overflow;
        },
        [this](bool muted) {
            desired_mute_ = muted;
            if (!microphone_busy_)
                apply_mute();
        });
}
void VendorActions::configure_display() {
    display_deadline_.setSingleShot(true);
    connect(&display_deadline_, &QTimer::timeout, this, [this] {
        osd_.present_refresh({application::display::Status::Unconfirmed, 0, "refresh-timeout"});
    });
    connect(&display_result_, &QFutureWatcher<application::display::Change>::finished, this,
            &VendorActions::complete_display);
}
void VendorActions::complete_microphone() {
    microphone_busy_ = false;
    microphone_deadline_.stop();
    // Coalesce state intent rather than dropping a mute received during another call.
    if (desired_mute_) {
        apply_mute();
        return;
    }
    if (microphone_elapsed_.elapsed() >= 10000)
        return;
    {
        const std::lock_guard lock(delivery_->mutex);
        if (delivery_->microphone_generation != microphone_generation_ || delivery_->overflow)
            return;
    }
    present_microphone_result();
}
void VendorActions::present_microphone_result() {
    try {
        const auto result = microphone_result_.result();
        if (!microphone_background_) {
            if (result.status == application::microphone::Status::Ready)
                publish_state_changed();
            osd_.present(result);
        }
    } catch (...) {
        if (!microphone_background_)
            osd_.present({application::microphone::Status::Failed, {}, "microphone-operation-failed"});
    }
}
void VendorActions::complete_display() {
    display_busy_ = false;
    display_deadline_.stop();
    if (display_elapsed_.elapsed() >= 10000)
        return;
    try {
        const auto result = display_result_.result();
        if (result.status == application::display::Status::Ready)
            publish_state_changed();
        osd_.present_refresh(result);
    } catch (...) {
        osd_.present_refresh({application::display::Status::Failed, 0, "refresh-operation-failed"});
    }
}
void VendorActions::enqueue(const std::shared_ptr<Delivery> &delivery,
                            shortcuts::VendorNotification notification) {
    const std::lock_guard lock(delivery->mutex);
    auto *receiver = delivery->receiver;
    if (!receiver || delivery->overflow)
        return;
    if (std::holds_alternative<shortcuts::FirmwareMicrophoneDesiredState>(notification))
        ++delivery->microphone_generation;
    if (delivery->pending.size() == 128) {
        delivery->pending.clear();
        delivery->overflow = true;
    } else {
        delivery->pending.push_back(notification);
    }
    if (!delivery->scheduled) {
        delivery->scheduled = true;
        QMetaObject::invokeMethod(receiver, [receiver] { receiver->drain(); }, Qt::QueuedConnection);
    }
}
void VendorActions::start_source() {
    const auto delivery = delivery_;
    const auto result = source_->start(
        [delivery](shortcuts::VendorNotification notification) { enqueue(delivery, notification); },
        [delivery](const shortcuts::VendorInputStatus &status) { source_status(delivery, status); });
    qInfo() << "Laptop key input starting:" << static_cast<int>(result.state);
}
void VendorActions::source_status(const std::shared_ptr<Delivery> &delivery,
                                  const shortcuts::VendorInputStatus &status) {
    qInfo() << "Laptop key input:" << static_cast<int>(status.state)
            << QString::fromStdString(status.diagnostic);
    if (status.state != shortcuts::VendorInputState::Ready) {
        if (status.state != shortcuts::VendorInputState::Starting) {
            const std::lock_guard lock(delivery->mutex);
            ++delivery->microphone_generation;
        }
        return;
    }
    const std::lock_guard lock(delivery->mutex);
    if (auto *receiver = delivery->receiver)
        QMetaObject::invokeMethod(
            receiver, [receiver] { receiver->read_initial_microphone(); }, Qt::QueuedConnection);
}
void VendorActions::read_initial_microphone() {
    initial_microphone_->start();
}
void VendorActions::profile_ready() {
    try {
        source_ = adapters::make_native_vendor_input(profile_result_.result());
        start_source();
    } catch (...) {
        qWarning() << "Laptop key input unavailable: native profile discovery failed";
    }
}
VendorActions::~VendorActions() {
    {
        const std::lock_guard lock(delivery_->mutex);
        delivery_->receiver = nullptr;
    }
    if (source_)
        (void)source_->stop();
}
void VendorActions::drain() {
    std::deque<shortcuts::VendorNotification> pending;
    bool overflow = false;
    {
        const std::lock_guard lock(delivery_->mutex);
        pending.swap(delivery_->pending);
        delivery_->scheduled = false;
        overflow = delivery_->overflow;
    }
    if (overflow) {
        (void)source_->stop();
        qWarning() << "Laptop key delivery overflow; input stopped, firmware state is unknown";
        return;
    }
    for (const auto &notification : pending)
        handle(notification);
}
void VendorActions::handle(const shortcuts::VendorNotification &notification) {
    preferences_.refresh();
    if (std::holds_alternative<shortcuts::VendorActivation>(notification)) {
        const auto action = static_cast<shortcuts::BindingAction>(preferences_.vendor_key_action());
        dispatch_application_action(
            action == shortcuts::BindingAction::Default ? shortcuts::BindingAction::OpenManager : action);
    } else if (const auto *profile = std::get_if<shortcuts::FirmwarePerformanceAnnouncement>(&notification)) {
        handle_profile(profile->announced_profile);
    } else if (std::holds_alternative<shortcuts::RefreshKeyNotification>(notification)) {
        handle_refresh();
    } else if (const auto *microphone =
                   std::get_if<shortcuts::FirmwareMicrophoneDesiredState>(&notification)) {
        desired_mute_ = microphone->muted;
        if (!microphone_busy_)
            apply_mute();
    }
}
void VendorActions::apply_mute() {
    if (!desired_mute_)
        return;
    desired_mute_.reset();
    microphone_busy_ = true;
    microphone_elapsed_.start();
    microphone_deadline_.start(10000);
    microphone_background_ = false;
    const auto control = microphone_;
    const auto firmware = microphone_firmware_;
    const auto current_intent = microphone_intent_guard();
    microphone_result_.setFuture(QtConcurrent::run(&work_.commands, [control, firmware, current_intent] {
        if (!firmware)
            return application::microphone::Observation{};
        return application::microphone::synchronize(*firmware, *control, current_intent);
    }));
}
std::function<bool()> VendorActions::microphone_intent_guard() {
    const auto delivery = delivery_;
    const std::lock_guard lock(delivery->mutex);
    microphone_generation_ = delivery->microphone_generation;
    return [delivery, generation = microphone_generation_] {
        const std::lock_guard guard(delivery->mutex);
        return delivery->receiver && !delivery->overflow && delivery->microphone_generation == generation;
    };
}
void VendorActions::reconcileMicrophone() {
    if (!microphone_firmware_ || microphone_busy_)
        return;
    desired_mute_ = true; // Schedules a GET; the value is never applied as endpoint intent.
    apply_mute();
    microphone_background_ = true;
}
void VendorActions::cycle_refresh() {
    if (display_busy_)
        return;
    display_busy_ = true;
    display_elapsed_.start();
    display_deadline_.start(10000);
    osd_.prepare_refresh();
    const auto control = display_;
    const auto selected = preferences_.refresh_cycle_rates();
    display_result_.setFuture(QtConcurrent::run(&work_.commands, [control, selected] {
        return application::display::cycle_internal_refresh(*control, selected);
    }));
}

void VendorActions::publish_state_changed() {
    if (state_changed_)
        state_changed_();
}

void VendorActions::handle_profile(std::uint8_t profile) {
    // This is a profile announcement, including software SETs, not a rebindable Fn+K press.
    publish_state_changed();
    osd_.present_profile(profile);
}

void VendorActions::handle_refresh() {
    if (!dispatch_application_action(static_cast<shortcuts::BindingAction>(preferences_.fn_s_action())))
        cycle_refresh();
}

bool VendorActions::dispatch_application_action(shortcuts::BindingAction action) {
    switch (action) {
    case shortcuts::BindingAction::Default:
        return false;
    case shortcuts::BindingAction::OpenManager:
        open_manager_();
        break;
    case shortcuts::BindingAction::OpenTray:
        open_tray_();
        break;
    case shortcuts::BindingAction::None:
    case shortcuts::BindingAction::Invalid:
        break;
    }
    return true;
}
} // namespace manager::session
