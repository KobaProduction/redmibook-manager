#include "apps/session/initial_microphone.hpp"
#include <QDebug>
#include <QtConcurrentRun>
namespace manager::session {
InitialMicrophoneRead::InitialMicrophoneRead(WorkQueues &work,
                                             std::shared_ptr<application::microphone::FirmwareStatePort> port,
                                             std::function<bool()> superseded, std::function<void(bool)> mute)
    : work_(work), port_(std::move(port)), superseded_(std::move(superseded)), mute_(std::move(mute)) {
    deadline_.setSingleShot(true);
    connect(&deadline_, &QTimer::timeout, this, [this] {
        expired_ = true;
        qWarning() << "Initial firmware microphone read timed out; endpoint state preserved";
        emit finished(false);
    });
    connect(&pending_, &QFutureWatcher<application::microphone::FirmwareState>::finished, this,
            &InitialMicrophoneRead::complete);
}
void InitialMicrophoneRead::start() {
    if (started_ || !port_)
        return;
    started_ = true;
    elapsed_.start();
    deadline_.start(10000);
    pending_.setFuture(QtConcurrent::run(&work_.observations, [port = port_] { return port->read(); }));
}
void InitialMicrophoneRead::complete() {
    deadline_.stop();
    if (expired_)
        return;
    if (elapsed_.elapsed() >= 10000) {
        emit finished(false);
        return;
    }
    bool requested = false;
    try {
        const auto state = pending_.result();
        qInfo() << "Initial firmware microphone state:" << static_cast<int>(state.status)
                << QString::fromStdString(state.detail);
        requested = application::microphone::apply_initial_state(state, superseded_());
        if (requested)
            mute_(*state.desired_muted);
    } catch (...) {
        qWarning() << "Initial firmware microphone read failed; endpoint state preserved";
    }
    emit finished(requested);
}
} // namespace manager::session
