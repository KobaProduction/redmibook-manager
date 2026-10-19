#include "apps/session/vendor_actions.hpp"
#include "adapters/device_profile/native_profile.hpp"
#include "adapters/platform/vendor_input.hpp"
#include "application/display/cycle_refresh.hpp"
#include <QDebug>
#include <QtConcurrent/QtConcurrentRun>

namespace manager::session {
namespace shortcuts = application::shortcuts;
VendorActions::VendorActions(presentation::Preferences &preferences, std::function<void()> open_manager,
                             std::function<void()> state_changed)
    : preferences_(preferences), delivery_(std::make_shared<Delivery>()),
      microphone_(std::make_shared<adapters::NativeMicrophone>()),
      display_(std::make_shared<adapters::NativeRefresh>()), microphone_osd_(preferences),
      vendor_osd_(preferences), open_manager_(std::move(open_manager)),
      state_changed_(std::move(state_changed)) {
    delivery_->receiver = this;
    configure_microphone();
    configure_display();
    connect(&profile_result_, &QFutureWatcher<application::DeviceProfile>::finished, this,
            &VendorActions::profile_ready);
    profile_result_.setFuture(QtConcurrent::run([] { return adapters::NativeDeviceProfile{}.read(); }));
}
void VendorActions::configure_microphone() {
    microphone_deadline_.setSingleShot(true);
    connect(&microphone_deadline_, &QTimer::timeout, this, [this] {
        microphone_osd_.present({application::microphone::Status::Failed, {}, "microphone-timeout"});
    });
    connect(&microphone_result_, &QFutureWatcher<application::microphone::Observation>::finished, this,
            &VendorActions::complete_microphone);
}
void VendorActions::configure_display() {
    display_deadline_.setSingleShot(true);
    connect(&display_deadline_, &QTimer::timeout, this, [this] {
        vendor_osd_.present_refresh({application::display::Status::Unconfirmed, 0, "refresh-timeout"});
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
    try {
        const auto result = microphone_result_.result();
        if (result.status == application::microphone::Status::Ready)
            publish_state_changed();
        microphone_osd_.present(result);
    } catch (...) {
        microphone_osd_.present({application::microphone::Status::Failed, {}, "microphone-operation-failed"});
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
        vendor_osd_.present_refresh(result);
    } catch (...) {
        vendor_osd_.present_refresh({application::display::Status::Failed, 0, "refresh-operation-failed"});
    }
}
void VendorActions::enqueue(const std::shared_ptr<Delivery> &delivery,
                            shortcuts::VendorNotification notification) {
    const std::lock_guard lock(delivery->mutex);
    auto *receiver = delivery->receiver;
    if (!receiver || delivery->overflow)
        return;
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
        [](const shortcuts::VendorInputStatus &status) {
            qInfo() << "Laptop key input:" << static_cast<int>(status.state)
                    << QString::fromStdString(status.diagnostic);
        });
    qInfo() << "Laptop key input starting:" << static_cast<int>(result.state);
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
        vendor_osd_.present_mi();
        open_manager_();
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
    const bool muted = *desired_mute_;
    desired_mute_.reset();
    microphone_busy_ = true;
    microphone_elapsed_.start();
    microphone_deadline_.start(10000);
    const auto control = microphone_;
    microphone_result_.setFuture(QtConcurrent::run([control, muted] { return control->set_muted(muted); }));
}
void VendorActions::cycle_refresh() {
    if (display_busy_)
        return;
    display_busy_ = true;
    display_elapsed_.start();
    display_deadline_.start(10000);
    vendor_osd_.prepare_refresh();
    const auto control = display_;
    const auto selected = preferences_.refresh_cycle_rates();
    display_result_.setFuture(QtConcurrent::run(
        [control, selected] { return application::display::cycle_internal_refresh(*control, selected); }));
}

void VendorActions::publish_state_changed() {
    if (state_changed_)
        state_changed_();
}

void VendorActions::handle_profile(std::uint8_t profile) {
    // Firmware has already acted before this notification. Rebinding cannot undo it.
    publish_state_changed();
    const auto action = static_cast<shortcuts::BindingAction>(preferences_.fn_k_action());
    if (action == shortcuts::BindingAction::OpenManager)
        open_manager_();
    else if (action == shortcuts::BindingAction::Default)
        vendor_osd_.present_profile(profile);
}

void VendorActions::handle_refresh() {
    const auto action = static_cast<shortcuts::BindingAction>(preferences_.fn_s_action());
    if (action == shortcuts::BindingAction::OpenManager)
        open_manager_();
    else if (action == shortcuts::BindingAction::Default)
        cycle_refresh();
}
} // namespace manager::session
