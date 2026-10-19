#include "adapters/storage/ui_preferences/store.hpp"
#include "presentation/device_controls_model.hpp"
#include <QApplication>
#include <QEventLoop>
#include <QMetaObject>
#include <QTemporaryDir>
#include <QThread>
#include <atomic>
#include <functional>
#include <future>
#include <memory>
#include <utility>

namespace {
namespace microphone = manager::application::microphone;
namespace display = manager::application::display;
using manager::presentation::DeviceControlsModel;
using manager::presentation::Preferences;

struct Gate {
    explicit Gate(QEventLoop &context) : gui(context), released(release.get_future().share()) {}
    void block() {
        worker.store(QThread::currentThread() != gui.thread());
        QMetaObject::invokeMethod(&gui, entered, Qt::QueuedConnection);
        released.wait();
    }
    void port_destroyed() {
        if (destroyed) {
            QMetaObject::invokeMethod(&gui, destroyed, Qt::QueuedConnection);
        }
    }
    QEventLoop &gui;
    std::promise<void> release;
    std::shared_future<void> released;
    std::function<void()> entered;
    std::function<void()> destroyed;
    std::atomic<bool> worker{false};
};

class BlockingMicrophone final : public microphone::ControlPort {
  public:
    explicit BlockingMicrophone(std::shared_ptr<Gate> gate) : gate_(std::move(gate)) {}
    ~BlockingMicrophone() override {
        gate_->port_destroyed();
    }
    microphone::Observation read() const override {
        gate_->block();
        return {microphone::Status::Ready, false, "fake"};
    }
    microphone::Observation set_muted(bool) const override {
        return {};
    }

  private:
    std::shared_ptr<Gate> gate_;
};

class FakeDisplay final : public display::RefreshPort {
  public:
    explicit FakeDisplay(std::shared_ptr<Gate> gate) : gate_(std::move(gate)) {}
    ~FakeDisplay() override {
        gate_->port_destroyed();
    }
    display::Snapshot read() const override {
        return {display::Status::Ready, {}, "fake"};
    }
    display::Change set_refresh(const std::string &, unsigned int) const override {
        return {};
    }

  private:
    std::shared_ptr<Gate> gate_;
};

struct ResponsiveCase {
    explicit ResponsiveCase(Preferences &preferences)
        : gate(std::make_shared<Gate>(loop)), model(std::make_shared<BlockingMicrophone>(gate),
                                                    std::make_shared<FakeDisplay>(gate), preferences) {
        gate->entered = [this] {
            responsive = model.busy() && gate->worker.load() && QThread::currentThread() == loop.thread();
            gate->release.set_value();
        };
        QObject::connect(
            &model, &DeviceControlsModel::changed, &loop, [this] { completed(); }, Qt::DirectConnection);
    }
    void completed() {
        if (QThread::currentThread() != loop.thread()) {
            QMetaObject::invokeMethod(&loop, &QEventLoop::quit, Qt::QueuedConnection);
            return;
        }
        if (!model.busy()) {
            gui_completion = model.microphoneAvailable() && !model.microphoneMuted();
            loop.quit();
        }
    }
    bool run() {
        model.refresh();
        loop.exec();
        return responsive && gui_completion;
    }
    QEventLoop loop;
    std::shared_ptr<Gate> gate;
    DeviceControlsModel model;
    bool responsive{false};
    bool gui_completion{false};
};

struct LifetimeCase {
    explicit LifetimeCase(Preferences &preferences) : gate(std::make_shared<Gate>(loop)) {
        auto microphone = std::make_shared<BlockingMicrophone>(gate);
        auto display = std::make_shared<FakeDisplay>(gate);
        microphone_lifetime = microphone;
        display_lifetime = display;
        model = std::make_unique<DeviceControlsModel>(microphone, display, preferences);
        gate->entered = [this] {
            model.reset();
            retained = !microphone_lifetime.expired() && !display_lifetime.expired() && gate->worker.load();
            gate->release.set_value();
        };
        gate->destroyed = [this] {
            if (++destroyed == 2) {
                loop.quit();
            }
        };
    }
    bool run() {
        model->refresh();
        loop.exec();
        return retained && destroyed == 2 && microphone_lifetime.expired() && display_lifetime.expired();
    }
    QEventLoop loop;
    std::shared_ptr<Gate> gate;
    std::unique_ptr<DeviceControlsModel> model;
    std::weak_ptr<BlockingMicrophone> microphone_lifetime;
    std::weak_ptr<FakeDisplay> display_lifetime;
    int destroyed{0};
    bool retained{false};
};
} // namespace

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    QTemporaryDir directory;
    manager::adapters::UiPreferenceStore store(directory.path());
    Preferences preferences(store);
    ResponsiveCase responsive(preferences);
    LifetimeCase lifetime(preferences);
    return responsive.run() && lifetime.run() ? 0 : 1;
}
