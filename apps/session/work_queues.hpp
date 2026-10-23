#pragma once
#include <QThreadPool>

namespace manager::session {
// Composition-owned pools outlive their controllers and captured native dependencies.
// Threads are created on demand and use Qt's default idle expiry; no polling worker.
struct WorkQueues final {
    WorkQueues() {
        commands.setMaxThreadCount(2);
        observations.setMaxThreadCount(3);
    }
    QThreadPool commands;
    QThreadPool observations;
};
} // namespace manager::session
