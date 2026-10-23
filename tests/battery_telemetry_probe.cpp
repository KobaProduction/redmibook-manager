#include "adapters/platform/battery.hpp"
#include <chrono>
#include <condition_variable>
#include <iostream>
#include <mutex>
#include <string_view>
namespace {
template <class T> void field(const char *name, const std::optional<T> &value) {
    std::cout << ",\"" << name << "\":";
    if (value)
        std::cout << *value;
    else
        std::cout << "null";
}
int power_events() {
    std::mutex mutex;
    std::condition_variable ready;
    int events{};
    auto subscription = manager::adapters::watch_power([&] {
        const std::lock_guard lock(mutex);
        ++events;
        ready.notify_one();
    });
    {
        std::unique_lock lock(mutex);
        ready.wait_for(lock, std::chrono::seconds(3), [&] { return events > 0; });
    }
    subscription.reset();
    return events;
}
void print_telemetry(const manager::application::battery::Telemetry &result) {
    field("percent", result.percent);
    field("online", result.online);
    field("charging", result.charging);
    field("discharging", result.discharging);
    field("remainingWh", result.remaining_wh);
    field("fullWh", result.full_wh);
    field("designWh", result.design_wh);
    field("voltage", result.voltage);
    field("rateW", result.rate_w);
}
} // namespace
int main(int argc, char *argv[]) {
    const auto events = power_events();
    const auto port = manager::adapters::make_battery_telemetry();
    const auto result = port->read();
    int samples{};
    const auto began = std::chrono::steady_clock::now();
    if (argc > 1 && std::string_view(argv[1]) == "--benchmark") {
        for (; samples < 200; ++samples)
            if (port->read().status != manager::application::battery::TelemetryStatus::Available)
                break;
    }
    const auto duration =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began).count();
    std::cout << "{\"status\":" << static_cast<int>(result.status) << ",\"powerEvents\":" << events
              << ",\"samples\":" << samples << ",\"sampleDurationMs\":" << duration;
    print_telemetry(result);
    std::cout << "}\n";
}
