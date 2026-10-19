#pragma once
#include "adapters/platform/session_controls.hpp"
#include "application/device_profile/profile_port.hpp"
#include "application/shortcuts/vendor_input_port.hpp"
#include "presentation/osd/microphone_osd.hpp"
#include "presentation/osd/vendor_osd.hpp"
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
    VendorActions(presentation::Preferences &preferences, std::function<void()> open_manager,
                  std::function<void()> state_changed = {});
    ~VendorActions() override;

  private:
    struct Delivery {
        std::mutex mutex;
        VendorActions *receiver{};
        std::deque<application::shortcuts::VendorNotification> pending;
        bool scheduled{false};
        bool overflow{false};
    };
    presentation::Preferences &preferences_;
    std::shared_ptr<Delivery> delivery_;
    std::shared_ptr<application::microphone::ControlPort> microphone_;
    std::shared_ptr<application::display::RefreshPort> display_;
    presentation::MicrophoneOsd microphone_osd_;
    presentation::VendorOsd vendor_osd_;
    QFutureWatcher<application::microphone::Observation> microphone_result_;
    QFutureWatcher<application::display::Change> display_result_;
    QFutureWatcher<application::DeviceProfile> profile_result_;
    QTimer microphone_deadline_;
    QTimer display_deadline_;
    QElapsedTimer microphone_elapsed_;
    QElapsedTimer display_elapsed_;
    std::optional<bool> desired_mute_;
    std::function<void()> open_manager_;
    std::function<void()> state_changed_;
    bool microphone_busy_{false};
    bool display_busy_{false};
    std::unique_ptr<application::shortcuts::VendorInputPort> source_;
    void handle(const application::shortcuts::VendorNotification &notification);
    void drain();
    void configure_microphone();
    void configure_display();
    void complete_microphone();
    void complete_display();
    void start_source();
    void profile_ready();
    static void enqueue(const std::shared_ptr<Delivery> &delivery,
                        application::shortcuts::VendorNotification notification);
    void apply_mute();
    void cycle_refresh();
    void handle_profile(std::uint8_t profile);
    void handle_refresh();
    void publish_state_changed();
};
} // namespace manager::session
