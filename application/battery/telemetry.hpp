#pragma once
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>

namespace manager::application::battery {
enum class TelemetryStatus : std::uint8_t { Available, NotImplemented, Unavailable, Failed };
struct Telemetry {
    TelemetryStatus status{TelemetryStatus::Unavailable};
    std::optional<bool> online, charging, discharging;
    std::optional<int> percent;
    std::optional<double> remaining_wh, full_wh, design_wh, voltage, rate_w;
    bool operator==(const Telemetry &) const = default;
};
// Read-only, synchronous; invoke on a worker. Missing driver values stay unknown.
class TelemetryPort {
  public:
    virtual ~TelemetryPort() = default;
    virtual Telemetry read() = 0;
};
class PowerSubscription {
  public:
    virtual ~PowerSubscription() = default;
};
} // namespace manager::application::battery
