#pragma once
#include "application/display/refresh_port.hpp"
#include "application/feedback/device_feedback.hpp"
#include "application/microphone/control_port.hpp"
#include "application/runtime/read_status.hpp"
#include "presentation/common/preferences.hpp"
#include "presentation/common/rounded_frame.hpp"
#include <QLabel>
#include <QTimer>
#include <cstdint>

namespace manager::presentation {
class OsdPresenter final : public RoundedFrame,
                           public application::DiagnosticFeedbackPort,
                           public application::feedback::Presenter {
    Q_OBJECT
  public:
    explicit OsdPresenter(Preferences &preferences);
    application::feedback::Delivery present(application::feedback::Event event) override;
    void present(const application::microphone::Observation &observation);
    void show_result(const application::DiagnosticReply &reply) override;
    void present_mi();
    void present_profile(std::uint8_t announced_profile);
    void prepare_refresh();
    void present_refresh(const application::display::Change &result);

  private:
    QString profile_text() const;
    enum class Kind : std::uint8_t { Mi, Profile, Refresh, Microphone, Diagnostic };
    Preferences &preferences_;
    QLabel icon_;
    QLabel text_;
    application::feedback::DistinctState distinct_;
    application::microphone::Observation microphone_;
    application::DiagnosticReply diagnostic_;
    QTimer timeout_;
    Kind kind_{Kind::Mi};
    std::uint8_t profile_{};
    application::display::Change refresh_;
    void retranslate();
    void update_icon();
    void reveal();
};
} // namespace manager::presentation
