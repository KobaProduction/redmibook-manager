#include "presentation/common/runtime_monitor.hpp"
#include <QPointer>

namespace manager::presentation {
RuntimeMonitor::RuntimeMonitor(application::SessionPort &port) : port_(port) {
    heartbeat_.setInterval(2000);
    connect(&heartbeat_, &QTimer::timeout, this, &RuntimeMonitor::refresh);
    heartbeat_.start();
    QTimer::singleShot(0, this, &RuntimeMonitor::refresh);
}
void RuntimeMonitor::refresh() {
    if (pending_) {
        return;
    }
    pending_ = true;
    emit changed();
    const QPointer<RuntimeMonitor> receiver(this);
    const bool started = port_.request(application::SessionCommand::Status,
                                       [receiver](const application::SessionReply &reply) {
                                           if (receiver) {
                                               receiver->accept(reply);
                                           }
                                       });
    if (!started) {
        pending_ = false;
        emit changed();
    }
}
void RuntimeMonitor::accept(const application::SessionReply &reply) {
    reply_ = reply;
    pending_ = false;
    emit changed();
}
QString RuntimeMonitor::status_text() const {
    switch (application::connection_health(reply_, {MANAGER_APP_VERSION, MANAGER_BUILD_ID})) {
    case application::ConnectionHealth::SessionUnavailable:
        return tr("Session runtime unavailable");
    case application::ConnectionHealth::CoreUnavailable:
        return tr("Core unavailable — no response");
    case application::ConnectionHealth::VersionMismatch:
        return tr("Component versions or builds differ. Update or restart the components.");
    case application::ConnectionHealth::Ready:
        return tr("Background components connected");
    }
    return {};
}
} // namespace manager::presentation
