#pragma once
#include "adapters/platform/input_binding.hpp"
#include "adapters/platform/session_controls.hpp"
#include "presentation/osd/microphone_osd.hpp"
#include <QElapsedTimer>
#include <QFutureWatcher>
#include <QString>
#include <QTimer>

namespace manager::session {
class InputActions final {
  public:
    explicit InputActions(presentation::Preferences &preferences);
    application::shortcuts::BindingResult bind_microphone(const QString &chord);

  private:
    std::shared_ptr<application::microphone::ControlPort> microphone_;
    presentation::MicrophoneOsd osd_;
    QFutureWatcher<application::microphone::Observation> pending_;
    QTimer deadline_;
    QElapsedTimer elapsed_;
    bool expired_{false};
    bool busy_{false};
    // Destroy subscription before callback dependencies.
    std::unique_ptr<application::shortcuts::InputPort> input_;
    void toggle_async();
    void complete();
};
} // namespace manager::session
