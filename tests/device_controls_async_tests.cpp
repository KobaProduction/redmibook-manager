#include "adapters/storage/ui_preferences/store.hpp"
#include "presentation/device_controls_model.hpp"
#include <QApplication>
#include <QEventLoop>
#include <QMetaObject>
#include <QTemporaryDir>
#include <QThread>
#include <QTimer>
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
        ++writes;
        return {};
    }
    mutable int writes{0};

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

namespace battery = manager::application::battery;
class ResetBattery final : public battery::ChargeControl {
  public:
    battery::Capability capability() const override {
        return {support, {battery::ChargeLimit::Full}, {}};
    }
    battery::Observation read() override {
        return {battery::Status::Available,
                matches ? battery::ChargeLimit::Full : battery::ChargeLimit::Protection80,
                {}};
    }
    battery::Observation request(battery::ChargeLimit value) override {
        ++writes;
        full_requested = value == battery::ChargeLimit::Full;
        worker = QThread::currentThread() != qApp->thread();
        return {reply, {}, {}};
    }
    battery::Status support{battery::Status::Available};
    battery::Status reply{battery::Status::AppliedUnconfirmed};
    bool matches{true};
    int writes{0};
    bool full_requested{false};
    bool worker{false};
};
bool offline_reset_rejected() {
    class OfflinePort final : public manager::application::PreferencePort {
      public:
        std::optional<manager::application::UiPreferences> read() const override {
            return manager::application::UiPreferences{};
        }
        manager::application::PreferenceConnection connection() const override {
            return manager::application::PreferenceConnection::Unavailable;
        }
        bool update(std::string_view, std::string_view) override {
            return false;
        }
        void observe(std::function<void()>) override {}
    } store;
    Preferences preferences(store);
    QEventLoop loop;
    const auto gate = std::make_shared<Gate>(loop);
    gate->release.set_value();
    const auto charge = std::make_shared<ResetBattery>();
    DeviceControlsModel model(std::make_shared<BlockingMicrophone>(gate), std::make_shared<FakeDisplay>(gate),
                              preferences, charge);
    bool rejected = false;
    QObject::connect(&model, &DeviceControlsModel::errorOccurred, &model,
                     [&](const QString &) { rejected = true; });
    model.prepareSettingsReset();
    return rejected && !model.busy() && charge->writes == 0;
}
struct ResetOptions {
    battery::Status reply;
    bool matches;
    bool supported;
};
class ResetCase {
  public:
    explicit ResetCase(ResetOptions options)
        : options_(options), store_(directory_.path()), preferences_(store_),
          gate_(std::make_shared<Gate>(loop_)), panel_(std::make_shared<FakeDisplay>(gate_)),
          charge_(std::make_shared<ResetBattery>()),
          model_(std::make_shared<BlockingMicrophone>(gate_), panel_, preferences_, charge_) {
        charge_->reply = options.reply;
        charge_->matches = options.matches;
        charge_->support = options.supported ? battery::Status::Available : battery::Status::NotImplemented;
        QObject::connect(&model_, &DeviceControlsModel::settingsResetReady, &loop_,
                         [this] { reset_ = preferences_.reset_defaults(); });
        QObject::connect(&model_, &DeviceControlsModel::changed, &loop_, [this] {
            if (!model_.busy())
                loop_.quit();
        });
    }
    bool run() {
        if (!preferences_.set_language(QStringLiteral("en")) ||
            !preferences_.set_refresh_cycle_rates({60, 120}))
            return false;
        QTimer::singleShot(3000, &loop_, &QEventLoop::quit);
        QTimer::singleShot(0, &model_, &DeviceControlsModel::prepareSettingsReset);
        loop_.exec();
        return validate();
    }

  private:
    ResetOptions options_;
    QTemporaryDir directory_;
    manager::adapters::UiPreferenceStore store_;
    Preferences preferences_;
    QEventLoop loop_;
    std::shared_ptr<Gate> gate_;
    std::shared_ptr<FakeDisplay> panel_;
    std::shared_ptr<ResetBattery> charge_;
    DeviceControlsModel model_;
    bool reset_{false};
    bool validate() const {
        const bool expected = !options_.supported ||
                              (options_.reply == battery::Status::AppliedUnconfirmed && options_.matches);
        const bool passed =
            !model_.busy() && reset_ == expected && panel_->writes == 0 &&
            preferences_.refresh_cycle_rates() == std::vector<unsigned int>{60, 120} &&
            preferences_.language() == (expected ? QStringLiteral("system") : QStringLiteral("en")) &&
            (options_.supported ? charge_->writes == 1 && charge_->full_requested && charge_->worker
                                : charge_->writes == 0);
        if (!passed)
            qCritical() << "Reset contract failed" << static_cast<int>(options_.reply) << options_.matches
                        << options_.supported << reset_ << expected << charge_->writes << model_.busy();
        return passed;
    }
};
class FirmwareCommand final : public microphone::FirmwareStatePort {
  public:
    mutable std::atomic<int> writes{};
    mutable std::atomic<bool> worker{false};
    QThread *gui{QThread::currentThread()};
    microphone::FirmwareState read() const override {
        return {};
    }
    microphone::FirmwareState request(bool muted) const override {
        ++writes;
        worker = QThread::currentThread() != gui;
        return {microphone::FirmwareStateStatus::Ready, muted, {}};
    }
};
bool microphone_command(Preferences &preferences) {
    QEventLoop loop;
    const auto firmware = std::make_shared<FirmwareCommand>();
    const auto gate = std::make_shared<Gate>(loop);
    DeviceControlsModel model(std::make_shared<BlockingMicrophone>(gate), std::make_shared<FakeDisplay>(gate),
                              preferences, nullptr, nullptr, nullptr, firmware);
    manager::application::overview::DeviceState state;
    state.microphone = {microphone::Status::Ready, false, {}};
    state.microphone_firmware = {microphone::FirmwareStateStatus::Ready, false, {}};
    model.acceptState(state);
    QObject::connect(&model, &DeviceControlsModel::changed, &loop, [&] {
        if (!model.busy())
            loop.quit();
    });
    QTimer::singleShot(2000, &loop, &QEventLoop::quit);
    model.setMicrophoneMuted(true);
    loop.exec();
    return !model.busy() && model.microphoneFirmwareMuted() && !model.microphoneMuted() &&
           firmware->writes == 1 && firmware->worker;
}
bool display_support(Preferences &preferences) {
    QEventLoop loop;
    const auto gate = std::make_shared<Gate>(loop);
    DeviceControlsModel model(std::make_shared<BlockingMicrophone>(gate), std::make_shared<FakeDisplay>(gate),
                              preferences);
    manager::application::overview::DeviceState state;
    state.displays.status = display::Status::NotImplemented;
    model.acceptState(state);
    if (model.displaySupported())
        return false;
    state.displays.status = display::Status::Ready;
    state.displays.displays.push_back({"internal", "Panel", 1920, 1080, 60, {60, 120}, true});
    model.acceptState(state);
    if (!model.displaySupported())
        return false;
    model.acceptState({});
    return model.displaySupported();
}
bool reset_case(battery::Status reply, bool matches, bool supported = true) {
    return ResetCase({reply, matches, supported}).run();
}
} // namespace

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    QTemporaryDir directory;
    manager::adapters::UiPreferenceStore store(directory.path());
    Preferences preferences(store);
    ResponsiveCase responsive(preferences);
    LifetimeCase lifetime(preferences);
    const bool response = responsive.run();
    const bool retained = lifetime.run();
    const bool reset = offline_reset_rejected() && reset_case(battery::Status::AppliedUnconfirmed, true) &&
                       reset_case(battery::Status::AppliedUnconfirmed, false) &&
                       reset_case(battery::Status::OutcomeUnknown, false) &&
                       reset_case(battery::Status::AppliedUnconfirmed, false, false);
    if (!response || !retained || !reset)
        qCritical("async contract response=%d lifetime=%d reset=%d", response, retained, reset);
    return response && retained && reset && microphone_command(preferences) && display_support(preferences)
               ? 0
               : 1;
}
