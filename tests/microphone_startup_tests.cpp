#include "apps/session/initial_microphone.hpp"
#include <QCoreApplication>
#include <QEventLoop>
#include <QSemaphore>
#include <QThread>
#include <array>
#include <atomic>
namespace {
namespace mic = manager::application::microphone;
struct Case {
    mic::FirmwareState state;
    bool newer_event{false};
    bool initially_muted{false};
    bool apply{false};
};
class Source final : public mic::FirmwareStatePort {
  public:
    QEventLoop &loop;
    mic::FirmwareState state;
    mutable QSemaphore release;
    mutable std::atomic<int> reads{};
    mutable std::atomic<bool> worker{false};
    std::function<void()> entered;
    Source(QEventLoop &context, mic::FirmwareState initial) : loop(context), state(std::move(initial)) {}
    mic::FirmwareState read() const override {
        ++reads;
        worker.store(QThread::currentThread() != loop.thread());
        QMetaObject::invokeMethod(&loop, entered, Qt::QueuedConnection);
        release.acquire();
        return state;
    }
};
class Scenario {
  public:
    explicit Scenario(const Case &value)
        : test_(value), source_(std::make_shared<Source>(loop_, value.state)), muted_(value.initially_muted),
          reader_(
              work_, source_, [this] { return newer_; },
              [this](bool muted) {
                  ++writes_;
                  muted_ = muted;
              }) {
        source_->entered = [this] {
            newer_ = test_.newer_event;
            source_->release.release();
        };
        QObject::connect(&reader_, &manager::session::InitialMicrophoneRead::finished, &loop_,
                         [this](bool applied) {
                             completed_ = true;
                             applied_ = applied;
                             loop_.quit();
                         });
        QTimer::singleShot(2000, &loop_, [this] {
            source_->release.release();
            loop_.quit();
        });
    }
    bool run() {
        reader_.start();
        reader_.start();
        loop_.exec();
        return completed_ && applied_ == test_.apply && writes_ == (test_.apply ? 1 : 0) &&
               muted_ == (test_.apply ? test_.state.desired_muted.value_or(test_.initially_muted)
                                      : test_.initially_muted) &&
               source_->reads == 1 && source_->worker;
    }

  private:
    Case test_;
    QEventLoop loop_;
    manager::session::WorkQueues work_;
    std::shared_ptr<Source> source_;
    bool newer_{false};
    bool muted_{false};
    bool completed_{false};
    bool applied_{false};
    int writes_{};
    manager::session::InitialMicrophoneRead reader_;
};
} // namespace
int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    using Status = mic::FirmwareStateStatus;
    const std::array cases{Case{{Status::Ready, true, {}}, false, false, true},
                           Case{{Status::Ready, true, {}}, true, false, false},
                           Case{{Status::Ready, false, {}}, false, true, true},
                           Case{{Status::Ready, {}, {}}, false, true, false},
                           Case{{Status::Unavailable, {}, {}}, false, false, false},
                           Case{{Status::Denied, {}, {}}, false, true, false}};
    for (const auto &test : cases)
        if (!Scenario(test).run())
            return 1;
    return 0;
}
