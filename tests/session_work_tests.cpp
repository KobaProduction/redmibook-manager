#include "apps/session/work_queues.hpp"
#include <QCoreApplication>
#include <QFutureWatcher>
#include <QSemaphore>
#include <QTimer>
#include <QtConcurrent/QtConcurrentRun>

namespace {
class BlockedObservation final {
  public:
    explicit BlockedObservation(manager::session::WorkQueues &work) {
        work.observations.setMaxThreadCount(1);
        pending_ = QtConcurrent::run(&work.observations, [this] {
            started_.release();
            release_.acquire();
        });
    }
    ~BlockedObservation() {
        release_.release();
        pending_.waitForFinished();
    }
    bool ready() {
        return started_.tryAcquire(1, 1000);
    }

  private:
    QSemaphore started_;
    QSemaphore release_;
    QFuture<void> pending_;
};

bool command_during_observation(QCoreApplication &app) {
    manager::session::WorkQueues work;
    BlockedObservation observation(work);
    if (!observation.ready())
        return false;
    QFutureWatcher<int> command;
    QObject::connect(&command, &QFutureWatcher<int>::finished, &app,
                     [&] { app.exit(command.result() == 42 ? 0 : 1); });
    QTimer deadline;
    deadline.setSingleShot(true);
    QObject::connect(&deadline, &QTimer::timeout, &app, [&] { app.exit(1); });
    deadline.start(1500);
    command.setFuture(QtConcurrent::run(&work.commands, [] { return 42; }));
    return app.exec() == 0;
}
} // namespace

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);
    return command_during_observation(app) ? 0 : 1;
}
