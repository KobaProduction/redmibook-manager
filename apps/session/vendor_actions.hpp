#pragma once
#include "adapters/platform/session_controls.hpp"
#include "application/device_profile/profile_port.hpp"
#include "application/microphone/firmware_state_port.hpp"
#include "application/shortcuts/binding_action.hpp"
#include "application/shortcuts/vendor_input_port.hpp"
#include "apps/session/initial_microphone.hpp"
#include "apps/session/work_queues.hpp"
#include "presentation/osd/osd_presenter.hpp"
#include <QElapsedTimer>
#include <QFutureWatcher>
#include <QObject>
#include <QTimer>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>

namespace manager::session {
// Session composition: firmware intent -> explicit Windows Audio action; profile
// announcements remain observations and never cause a second firmware write.
class VendorActions final : public QObject {
  public:
    VendorActions(presentation::Preferences &preferences, presentation::OsdPresenter &osd, WorkQueues &work,
                  std::function<void()> open_manager, std::function<void()> open_tray,
                  std::function<void()> state_changed = {});
    ~VendorActions() override;
    void reconcileMicrophone();

  private:
    struct Delivery {
        std::mutex mutex;
        VendorActions *receiver{};
        std::deque<application::shortcuts::VendorNotification> pending;
        bool scheduled{false};
        bool overflow{false};
        std::uint64_t microphone_generation{};
    };
    WorkQueues &work_;
    presentation::Preferences &preferences_;
    std::shared_ptr<Delivery> delivery_;
    std::shared_ptr<application::microphone::ControlPort> microphone_;
    std::shared_ptr<application::microphone::FirmwareStatePort> microphone_firmware_;
    std::shared_ptr<application::display::RefreshPort> display_;
    presentation::OsdPresenter &osd_;
    QFutureWatcher<application::microphone::Observation> microphone_result_;
    QFutureWatcher<application::display::Change> display_result_;
    QFutureWatcher<application::DeviceProfile> profile_result_;
    std::unique_ptr<InitialMicrophoneRead> initial_microphone_;
    QTimer microphone_deadline_;
    QTimer display_deadline_;
    QElapsedTimer microphone_elapsed_;
    QElapsedTimer display_elapsed_;
    std::optional<bool> desired_mute_;
    std::function<void()> open_manager_;
    std::function<void()> open_tray_;
    bool dispatch_application_action(application::shortcuts::BindingAction action);
    std::function<void()> state_changed_;
    bool microphone_busy_{false};
    bool microphone_background_{false};
    std::uint64_t microphone_generation_{};
    bool display_busy_{false};
    std::unique_ptr<application::shortcuts::VendorInputPort> source_;
    void handle(const application::shortcuts::VendorNotification &notification);
    void drain();
    void configure_microphone();
    void configure_initial_microphone();
    void configure_display();
    void complete_microphone();
    void present_microphone_result();
    void complete_display();
    void start_source();
    void profile_ready();
    void read_initial_microphone();
    static void source_status(const std::shared_ptr<Delivery> &delivery,
                              const application::shortcuts::VendorInputStatus &status);
    static void enqueue(const std::shared_ptr<Delivery> &delivery,
                        application::shortcuts::VendorNotification notification);
    void apply_mute();
    std::function<bool()> microphone_intent_guard();
    void cycle_refresh();
    void handle_profile(std::uint8_t profile);
    void handle_refresh();
    void publish_state_changed();
};
} // namespace manager::session
