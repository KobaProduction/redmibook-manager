#include "adapters/platform/power_controls.hpp"
#include "adapters/windows/battery/power_notification.hpp"
#include <windows.h>

#include <atomic>
#include <bit>
#include <cstring>
#include <mutex>
#include <powrprof.h>
#include <vector>

namespace manager::adapters {
namespace {
class SaverListener final : public application::power::SaverObservation {
  public:
    struct Slot {
        std::mutex mutex;
        std::function<void()> callback;
        void notify() {
            const std::lock_guard lock(mutex);
            if (callback)
                callback();
        }
    };
    std::shared_ptr<Slot> slot{std::make_shared<Slot>()};
    ~SaverListener() override {
        const std::lock_guard lock(slot->mutex);
        slot->callback = {};
    }
};

class SaverEvents {
    std::mutex mutex_;
    std::vector<std::weak_ptr<SaverListener::Slot>> slots_;

  public:
    std::unique_ptr<application::power::SaverObservation> observe(std::function<void()> callback) {
        auto listener = std::make_unique<SaverListener>();
        listener->slot->callback = std::move(callback);
        const std::lock_guard lock(mutex_);
        slots_.push_back(listener->slot);
        return listener;
    }
    void notify() {
        const std::lock_guard lock(mutex_);
        std::erase_if(slots_, [](const auto &slot) { return slot.expired(); });
        for (const auto &slot : slots_)
            if (const auto listener = slot.lock())
                listener->notify();
    }
};

class ObservedState {
    static constexpr GUID setting{
        0x550e8400, 0xe29b, 0x41d4, {0xa7, 0x16, 0x44, 0x66, 0x55, 0x44, 0x00, 0x00}};
    std::atomic<int> value_{-1};
    HANDLE changed_{CreateEventW(nullptr, TRUE, FALSE, nullptr)};
    HPOWERNOTIFY subscription_{};
    windows_battery::NotifyParameters parameters_{};
    SaverEvents &events_;
    static ULONG CALLBACK changed(void *context, ULONG type, void *data) {
        if (!context || !data || type != PBT_POWERSETTINGCHANGE)
            return ERROR_SUCCESS;
        const auto *event = static_cast<POWERBROADCAST_SETTING *>(data);
        if (!IsEqualGUID(event->PowerSetting, setting) || event->DataLength != sizeof(DWORD))
            return ERROR_SUCCESS;
        DWORD value{};
        std::memcpy(&value, event->Data, sizeof(value));
        if (value > 2)
            return ERROR_INVALID_DATA;
        auto &self = *static_cast<ObservedState *>(context);
        const auto previous = self.value_.exchange(static_cast<int>(value));
        SetEvent(self.changed_);
        if (previous != static_cast<int>(value))
            self.events_.notify();
        return ERROR_SUCCESS;
    }

  public:
    explicit ObservedState(SaverEvents &events) : events_(events) {
        if (!changed_)
            return;
        parameters_.callback = changed;
        parameters_.context = this;
        PowerSettingRegisterNotification(&setting, windows_battery::callback_notification, &parameters_,
                                         &subscription_);
    }
    ~ObservedState() {
        if (subscription_)
            PowerSettingUnregisterNotification(subscription_);
        if (changed_)
            CloseHandle(changed_);
    }
    ObservedState(const ObservedState &) = delete;
    ObservedState &operator=(const ObservedState &) = delete;
    bool wait(bool enabled, DWORD timeout) const {
        const auto deadline = GetTickCount64() + timeout;
        for (;;) {
            ResetEvent(changed_);
            const auto current = value_.load();
            if (current >= 0 && (current > 0) == enabled)
                return true;
            const auto now = GetTickCount64();
            if (now >= deadline ||
                WaitForSingleObject(changed_, static_cast<DWORD>(deadline - now)) != WAIT_OBJECT_0)
                return false;
        }
    }

    std::optional<bool> read() const {
        if (!subscription_)
            return {};
        if (value_.load() < 0)
            WaitForSingleObject(changed_, 1500);
        const auto value = value_.load();
        return value < 0 ? std::optional<bool>{} : std::optional<bool>{value > 0};
    }
    void synchronize(bool enabled) {
        const auto previous = value_.exchange(enabled ? 1 : 0);
        if (previous < 0 || (previous > 0) != enabled)
            events_.notify();
    }
};
class ManualOverride {
    static constexpr ULONGLONG setting = 0x41c6013da3bc3075ULL;
    using Query = LONG(NTAPI *)(const ULONGLONG *, const void *, const void *, ULONG *, void *, ULONG *);
    using Update = LONG(NTAPI *)(const ULONGLONG *, const void *, ULONG, const void *, const void *, ULONG,
                                 ULONG);
    struct Snapshot {
        ULONG value;
        ULONG stamp;
    };
    template <class Function> static Function resolve(const char *name) {
        const auto module = GetModuleHandleW(L"ntdll.dll");
        return module ? std::bit_cast<Function>(GetProcAddress(module, name)) : nullptr;
    }
    Query query_{resolve<Query>("NtQueryWnfStateData")};
    Update update_{resolve<Update>("NtUpdateWnfStateData")};
    std::optional<Snapshot> snapshot() const {
        if (!query_)
            return {};
        Snapshot result{};
        ULONG length = sizeof(result.value);
        const auto status = query_(&setting, nullptr, nullptr, &result.stamp, &result.value, &length);
        if (status < 0 || length != sizeof(result.value) || (result.value != 1 && result.value != 2))
            return {};
        return result;
    }

  public:
    bool available() const {
        return update_ && snapshot().has_value();
    }
    application::power::SaverStatus request(bool enabled) const {
        using application::power::SaverStatus;
        const auto before = snapshot();
        if (!update_ || !before)
            return SaverStatus::NotImplemented;
        const ULONG value = enabled ? 1U : 2U;
        if (before->value == value)
            return SaverStatus::Ready;
        const auto status = update_(&setting, &value, sizeof(value), nullptr, nullptr, before->stamp, TRUE);
        if (status >= 0)
            return SaverStatus::Ready;
        return static_cast<ULONG>(status) == 0xc0000022U ? SaverStatus::Denied : SaverStatus::Unconfirmed;
    }
};

class Saver final : public application::power::SaverPort {
    mutable SaverEvents events_;
    mutable std::once_flag initialized_;
    mutable std::unique_ptr<ObservedState> observed_;
    ManualOverride manual_;
    std::mutex command_;

  public:
    application::power::Saver reconcile() const override {
        // A fresh registration delivers Windows' current initial setting even if
        // the long-lived subscription missed its last notification.
        read();
        SaverEvents quiet;
        ObservedState snapshot(quiet);
        const auto enabled = snapshot.read();
        if (!enabled)
            return {};
        observed_->synchronize(*enabled);
        return {application::power::SaverStatus::Ready, enabled, manual_.available()};
    }
    application::power::Saver read() const override {
        std::call_once(initialized_, [this] { observed_ = std::make_unique<ObservedState>(events_); });
        const auto enabled = observed_->read();
        if (!enabled.has_value())
            return {};
        return {application::power::SaverStatus::Ready, enabled, manual_.available()};
    }
    application::power::Saver set(bool enabled) override {
        const std::lock_guard lock(command_);
        auto result = read();
        if (!result.controllable)
            return {application::power::SaverStatus::NotImplemented, result.enabled, false};
        result.status = manual_.request(enabled);
        if (result.status == application::power::SaverStatus::Ready && !observed_->wait(enabled, 2500))
            result.status = application::power::SaverStatus::Unconfirmed;
        result.enabled = observed_->read();
        return result;
    }
    std::unique_ptr<application::power::SaverObservation> observe(std::function<void()> changed) override {
        return events_.observe(std::move(changed));
    }
};
} // namespace
std::shared_ptr<application::power::SaverPort> make_energy_saver() {
    return std::make_shared<Saver>();
}
} // namespace manager::adapters
