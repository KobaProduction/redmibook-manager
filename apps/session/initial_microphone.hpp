#pragma once
#include "application/microphone/firmware_state_port.hpp"
#include "apps/session/work_queues.hpp"
#include <QElapsedTimer>
#include <QFutureWatcher>
#include <QObject>
#include <QTimer>
#include <functional>
#include <memory>
namespace manager::session {
// Started only after native input subscription is ready. No firmware writes.
class InitialMicrophoneRead final : public QObject {
    Q_OBJECT
  public:
    InitialMicrophoneRead(WorkQueues &work, std::shared_ptr<application::microphone::FirmwareStatePort> port,
                          std::function<bool()> superseded, std::function<void(bool)> mute);
    void start();
  signals:
    // Query completed; true means synchronization was requested, not hardware confirmation.
    void finished(bool mute_requested);

  private:
    WorkQueues &work_;
    std::shared_ptr<application::microphone::FirmwareStatePort> port_;
    std::function<bool()> superseded_;
    std::function<void(bool)> mute_;
    QFutureWatcher<application::microphone::FirmwareState> pending_;
    QTimer deadline_;
    QElapsedTimer elapsed_;
    bool started_{false};
    bool expired_{false};
    void complete();
};
} // namespace manager::session
