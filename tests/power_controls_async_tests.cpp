#include "presentation/power_controls_model.hpp"
#include <QCoreApplication>
#include <QEventLoop>
#include <QThread>
#include <QTimer>
#include <atomic>
#include <mutex>

namespace {
namespace power = manager::application::power;
using manager::presentation::PowerControlsModel;

struct Listener final : power::SaverObservation {
    struct Slot {
        std::mutex mutex;
        std::function<void()> callback;
    };
    std::shared_ptr<Slot> slot;
    explicit Listener(std::shared_ptr<Slot> value) : slot(std::move(value)) {}
    ~Listener() override {
        const std::lock_guard lock(slot->mutex);
        slot->callback = {};
    }
};
class Saver final : public power::SaverPort {
  public:
    std::shared_ptr<Listener::Slot> slot{std::make_shared<Listener::Slot>()};
    std::atomic<bool> enabled{false};
    mutable std::atomic<bool> cached{false};
    mutable std::atomic<int> reads{0};
    std::atomic<int> writes{0};
    mutable std::atomic<bool> gui_read{false};
    std::function<void()> after_second_read;
    power::Saver read() const override {
        const bool value = cached.load();
        gui_read.store(gui_read.load() || QThread::currentThread() == QCoreApplication::instance()->thread());
        if (++reads == 2 && after_second_read)
            after_second_read();
        return {power::SaverStatus::Ready, value, true};
    }
    power::Saver reconcile() const override {
        cached.store(enabled.load());
        return read();
    }
    power::Saver set(bool value) override {
        ++writes;
        enabled.store(value);
        notify();
        return {power::SaverStatus::Ready, value, true};
    }
    void notify() {
        cached.store(enabled.load());
        const std::lock_guard lock(slot->mutex);
        if (slot->callback)
            slot->callback();
    }
    std::unique_ptr<power::SaverObservation> observe(std::function<void()> callback) override {
        slot->callback = std::move(callback);
        return std::make_unique<Listener>(slot);
    }
};
bool await(PowerControlsModel &model, const std::function<bool()> &ready) {
    if (ready())
        return true;
    QEventLoop loop;
    QTimer deadline;
    deadline.setSingleShot(true);
    QObject::connect(&deadline, &QTimer::timeout, &loop, &QEventLoop::quit);
    QObject::connect(&model, &PowerControlsModel::changed, &loop, [&] {
        if (ready())
            loop.quit();
    });
    deadline.start(1500);
    loop.exec();
    return ready();
}
void install_notification(Saver &saver, PowerControlsModel &model) {
    saver.after_second_read = [&saver, &model] {
        QMetaObject::invokeMethod(
            &model,
            [&saver] {
                saver.enabled.store(true);
                saver.notify();
            },
            Qt::QueuedConnection);
    };
}
int reconcile_and_command(PowerControlsModel &model, Saver &saver, const int &changes) {
    // Lose a notification deliberately; the ten-second reconciliation recovers it.
    auto *reconciliation = model.findChild<QTimer *>(QStringLiteral("powerReconciliation"));
    if (!reconciliation || reconciliation->interval() != 10000 || !reconciliation->isActive())
        return 3;
    saver.enabled.store(false);
    QMetaObject::invokeMethod(reconciliation, "timeout");
    if (!await(model, [&] { return !model.saverEnabled(); }) || changes != 3 || saver.writes != 0)
        return 4;
    model.setSaver(true);
    if (!model.busy() || !await(model, [&] { return model.saverEnabled() && !model.busy(); }) ||
        saver.writes != 1)
        return 5;
    return 0;
}
struct ChangeCounter : QObject {
    int changes{};
    bool background_busy{false};
    explicit ChangeCounter(PowerControlsModel &model) {
        QObject::connect(&model, &PowerControlsModel::changed, this, [this, &model] {
            ++changes;
            background_busy = background_busy || model.busy();
        });
    }
};
} // namespace

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    auto saver = std::make_shared<Saver>();
    {
        PowerControlsModel model({}, saver);
        ChangeCounter counter(model);
        if (!await(model, [&] { return model.saverAvailable(); }))
            return 1;
        install_notification(*saver, model);
        model.refresh(); // Same state: must not disable controls or publish a UI change.
        if (model.busy() || !await(model, [&] { return model.saverEnabled(); }) || counter.background_busy ||
            counter.changes != 2 || saver->writes != 0 || saver->gui_read)
            return 2;
        if (const auto result = reconcile_and_command(model, *saver, counter.changes))
            return result;
    }
    const std::lock_guard lock(saver->slot->mutex);
    return saver->slot->callback ? 6 : 0;
}
