#include "adapters/platform/battery.hpp"
namespace manager::adapters {
namespace {
class Battery final : public application::battery::TelemetryPort {
    application::battery::Telemetry read() override {
        application::battery::Telemetry result;
        result.status = application::battery::TelemetryStatus::NotImplemented;
        return result;
    }
};
} // namespace
std::shared_ptr<application::battery::TelemetryPort> make_battery_telemetry() {
    return std::make_shared<Battery>();
}
std::unique_ptr<application::battery::PowerSubscription> watch_power(const std::function<void()> &, bool) {
    return {};
}
} // namespace manager::adapters
