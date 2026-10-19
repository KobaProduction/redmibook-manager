#include "apps/session/input_actions.hpp"
#include <QStringList>
#include <QtConcurrent/QtConcurrentRun>

namespace manager::session {
InputActions::InputActions(presentation::Preferences &preferences)
    : microphone_(std::make_shared<adapters::NativeMicrophone>()), osd_(preferences),
      input_(adapters::make_native_input()) {
    deadline_.setSingleShot(true);
    QObject::connect(&deadline_, &QTimer::timeout, &osd_, [this] {
        expired_ = true;
        osd_.present({application::microphone::Status::Failed, {}, "microphone-timeout"});
    });
    QObject::connect(&pending_, &QFutureWatcher<application::microphone::Observation>::finished, &osd_,
                     [this] { complete(); });
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
        osd_.present(pending_.result());
    } catch (...) {
        osd_.present({application::microphone::Status::Failed, {}, "microphone-operation-failed"});
    }
}

void InputActions::toggle_async() {
    if (busy_) {
        return;
    }
    expired_ = false;
    busy_ = true;
    elapsed_.start();
    deadline_.start(10000);
    const auto control = microphone_;
    pending_.setFuture(QtConcurrent::run([control] { return application::microphone::toggle(*control); }));
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
