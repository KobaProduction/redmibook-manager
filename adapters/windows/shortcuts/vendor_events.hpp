#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace manager::adapters::windows_shortcuts {

enum class VendorProbeStatus : std::uint8_t {
    Completed,
    PermissionDenied,
    Unavailable,
    Failed,
    InvalidRequest
};

struct VendorEvent {
    std::uint64_t elapsed_ms{};
    std::vector<std::uint8_t> detail;
};

struct VendorEventReport {
    VendorProbeStatus status{VendorProbeStatus::Unavailable};
    bool subscribed{false};
    bool event_limit_reached{false};
    std::vector<VendorEvent> events;
    std::string detail;
};

// Passive HID_EVENT20 only. No keyboard hook, input injection, SET or action mapping.
// Synchronous diagnostic/worker boundary: Next waits for real events until the remaining
// observation deadline, never polls with Sleep. The callback is borrowed for this call
// and runs after subscription succeeds. Empty events do not prove Mi-button support.
[[nodiscard]] VendorEventReport probe_vendor_events(unsigned int timeout_ms,
                                                    const std::function<void()> &ready = {});

} // namespace manager::adapters::windows_shortcuts
