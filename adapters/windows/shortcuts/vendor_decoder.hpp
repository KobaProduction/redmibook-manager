#pragma once

#include "application/device_profile/profile_port.hpp"
#include "application/shortcuts/vendor_activation.hpp"

#include <chrono>
#include <cstdint>
#include <optional>
#include <span>

namespace manager::adapters::windows_shortcuts {

class VendorEventDecoder final {
  public:
    explicit VendorEventDecoder(const application::DeviceProfile &profile);
    [[nodiscard]] bool supported() const noexcept;
    // Thread-confined to the event source. Timestamp is monotonic observation time.
    // Exact P0A0A capture: leading 0x18 then trailing 0x19, both value1; one activation.
    // No press/release, hold or double-click semantics are inferred from these bytes.
    [[nodiscard]] std::optional<application::shortcuts::VendorNotification>
    consume(std::span<const std::uint8_t> packet, std::chrono::milliseconds observed_at);
    void reset() noexcept;

  private:
    bool valid_time(std::chrono::milliseconds observed_at);
    bool supported_{false};
    std::optional<std::chrono::milliseconds> leading_;
    std::optional<std::chrono::milliseconds> last_observation_;
    std::uint64_t sequence_{};
};

} // namespace manager::adapters::windows_shortcuts
