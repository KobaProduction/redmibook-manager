#pragma once
#include "adapters/platform/input_binding.hpp"
#include "application/microphone/firmware_state_port.hpp"
#include "apps/session/work_queues.hpp"
#include "presentation/osd/osd_presenter.hpp"
#include <QElapsedTimer>
#include <QFutureWatcher>
#include <QString>
#include <QTimer>

namespace manager::session {
class InputActions final {
  public:
    InputActions(presentation::OsdPresenter &osd, WorkQueues &work);
    ~InputActions();
    application::shortcuts::BindingResult bind_microphone(const QString &chord);

  private:
    WorkQueues &work_;
    std::shared_ptr<application::microphone::FirmwareStatePort> microphone_;
    presentation::OsdPresenter &osd_;
    QFutureWatcher<application::microphone::FirmwareState> pending_;
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
