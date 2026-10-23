#include "adapters/platform/battery.hpp"
#include "adapters/windows/battery/power_notification.hpp"
#include <powrprof.h>
#include <utility>
#include <windows.h>

namespace manager::adapters {
namespace {
using windows_battery::callback_notification;
using windows_battery::NotifyParameters;
constexpr GUID ac_source{0x5d3e9a59, 0xe9d5, 0x4b00, {0xa6, 0xbd, 0xff, 0x34, 0xff, 0x51, 0x65, 0x48}};
constexpr GUID battery_percent{0xa7ad8041, 0xb45a, 0x4cae, {0x87, 0xa3, 0xee, 0xcb, 0xb4, 0x68, 0xa9, 0xe1}};
class PowerWatch final : public application::battery::PowerSubscription {
  public:
    PowerWatch(std::function<void()> callback, bool observe_level) : callback_(std::move(callback)) {
        parameters_.callback = notified;
        parameters_.context = this;
        PowerSettingRegisterNotification(&ac_source, callback_notification, &parameters_, &source_);
        if (observe_level)
            PowerSettingRegisterNotification(&battery_percent, callback_notification, &parameters_, &level_);
        PowerRegisterSuspendResumeNotification(callback_notification, &parameters_, &resume_);
    }
    ~PowerWatch() override {
        if (source_)
            PowerSettingUnregisterNotification(source_);
        if (level_)
            PowerSettingUnregisterNotification(level_);
        if (resume_)
            PowerUnregisterSuspendResumeNotification(resume_);
    }

  private:
    static ULONG CALLBACK notified(PVOID context, ULONG type, PVOID) noexcept {
        if (type == PBT_POWERSETTINGCHANGE || type == PBT_APMRESUMEAUTOMATIC ||
            type == PBT_APMRESUMESUSPEND) {
            try {
                static_cast<PowerWatch *>(context)->callback_();
            } catch (...) {
                OutputDebugStringA("Redmibook power notification callback failed\n");
            }
        }
        return ERROR_SUCCESS;
    }
    std::function<void()> callback_;
    NotifyParameters parameters_{};
    HPOWERNOTIFY source_{}, level_{}, resume_{};
};
} // namespace
std::unique_ptr<application::battery::PowerSubscription> watch_power(std::function<void()> changed,
                                                                     bool observe_level) {
    return std::make_unique<PowerWatch>(std::move(changed), observe_level);
}
} // namespace manager::adapters
