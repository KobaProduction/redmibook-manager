#include "apps/session/input_actions.hpp"
#include "adapters/platform/firmware_controls.hpp"
#include <QStringList>
#include <QtConcurrent/QtConcurrentRun>

namespace manager::session {
InputActions::InputActions(presentation::OsdPresenter &osd, WorkQueues &work)
    : work_(work), microphone_(adapters::make_core_microphone_state()), osd_(osd),
      input_(adapters::make_native_input()) {
    deadline_.setSingleShot(true);
    QObject::connect(&deadline_, &QTimer::timeout, &osd_, [this] {
        expired_ = true;
        osd_.present({application::microphone::Status::Failed, {}, "microphone-timeout"});
    });
    QObject::connect(&pending_, &QFutureWatcher<application::microphone::FirmwareState>::finished, &osd_,
                     [this] { complete(); });
}
InputActions::~InputActions() {
    QObject::disconnect(&deadline_, nullptr, &osd_, nullptr);
    QObject::disconnect(&pending_, nullptr, &osd_, nullptr);
}
void InputActions::complete() {
    busy_ = false;
    deadline_.stop();
    if (expired_) {
        return;
    }
    if (elapsed_.elapsed() >= 10000) {
        osd_.present({application::microphone::Status::Failed, {}, "microphone-timeout"});
        return;
    }
    try {
        const auto result = pending_.result();
        if (result.status != application::microphone::FirmwareStateStatus::Ready)
            osd_.present({application::microphone::Status::Failed, {}, result.detail});
        // Successful SET emits 0x21; VendorActions alone owns endpoint sync and OSD.
    } catch (...) {
        osd_.present({application::microphone::Status::Failed, {}, "microphone-operation-failed"});
    }
}

void InputActions::toggle_async() {
    if (busy_ || !microphone_) {
        return;
    }
    expired_ = false;
    busy_ = true;
    elapsed_.start();
    deadline_.start(10000);
    const auto control = microphone_;
    pending_.setFuture(QtConcurrent::run(&work_.commands, [control] {
        auto state = control->read();
        if (state.status != application::microphone::FirmwareStateStatus::Ready || !state.desired_muted)
            return state;
        return control->request(!*state.desired_muted);
    }));
}

application::shortcuts::BindingResult InputActions::bind_microphone(const QString &chord) {
    application::shortcuts::KeyChord key;
    const auto parts = chord.toUpper().split(QLatin1Char('+'));
    for (const auto &part : parts) {
        if (part == QStringLiteral("CTRL") && !key.control) {
            key.control = true;
        } else if (part == QStringLiteral("ALT") && !key.alt) {
            key.alt = true;
        } else if (part == QStringLiteral("SHIFT") && !key.shift) {
            key.shift = true;
        } else if (part.size() == 1 && part.front() >= QLatin1Char('A') && part.front() <= QLatin1Char('Z') &&
                   !key.latin_key) {
            key.latin_key = part.front().toLatin1();
        } else {
            return {application::shortcuts::BindingStatus::Invalid, "invalid-key-chord"};
        }
    }
    return input_->bind(key, [this] { toggle_async(); });
}
} // namespace manager::session
