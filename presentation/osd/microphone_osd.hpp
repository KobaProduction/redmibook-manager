#pragma once
#include "application/microphone/control_port.hpp"
#include "presentation/common/preferences.hpp"
#include "presentation/common/rounded_frame.hpp"
#include <QLabel>
#include <QTimer>

namespace manager::presentation {
class MicrophoneOsd final : public RoundedFrame {
    Q_OBJECT
  public:
    explicit MicrophoneOsd(Preferences &preferences);
    void present(const application::microphone::Observation &observation);

  private:
    Preferences &preferences_;
    QLabel icon_;
    QLabel text_;
    QTimer timeout_;
    application::microphone::Observation observation_;
    void retranslate();
};
} // namespace manager::presentation
