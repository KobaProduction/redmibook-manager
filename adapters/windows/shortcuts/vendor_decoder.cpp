#include "adapters/windows/shortcuts/vendor_decoder.hpp"

#include <algorithm>

namespace manager::adapters::windows_shortcuts {
namespace {

// Correlation bound, not a claim about physical hold duration. Observed pairs were
// 55..90 ms apart; no missing edge may remain armed indefinitely across reconnects.
constexpr auto pair_window = std::chrono::milliseconds(1000);

bool qualified(const application::DeviceProfile &profile) {
    const bool observed = profile.evidence == application::ProfileEvidence::Observed ||
                          profile.evidence == application::ProfileEvidence::Verified;
    return observed && profile.status == application::ProfileReadStatus::Available &&
           profile.manufacturer == "XIAOMI" && profile.board_model == "TM2309" &&
           profile.bios_version == "RMAMT6B0P0A0A";
}

bool recognized(std::span<const std::uint8_t> packet) {
    return packet.size() == 32 && packet[0] == 1 &&
           std::ranges::all_of(packet.subspan(3), [](std::uint8_t byte) { return byte == 0; });
}

std::optional<application::shortcuts::VendorNotification> announcement(std::span<const std::uint8_t> packet) {
    using namespace application::shortcuts;
    if (packet[1] == 0x16 && packet[2] >= 1 && packet[2] <= 3)
        return FirmwarePerformanceAnnouncement{packet[2]};
    if (packet[1] == 0x1A && packet[2] == 0)
        return RefreshKeyNotification{};
    if (packet[1] == 0x21 && packet[2] <= 1)
        return FirmwareMicrophoneDesiredState{packet[2] == 0};
    return {};
}

} // namespace

VendorEventDecoder::VendorEventDecoder(const application::DeviceProfile &profile)
    : supported_(qualified(profile)) {}

bool VendorEventDecoder::supported() const noexcept {
    return supported_;
}

void VendorEventDecoder::reset() noexcept {
    leading_.reset();
    last_observation_.reset();
}

bool VendorEventDecoder::valid_time(std::chrono::milliseconds observed_at) {
    if (observed_at.count() < 0 || (last_observation_ && observed_at < *last_observation_)) {
        reset();
        return false;
    }
    last_observation_ = observed_at;
    return true;
}

std::optional<application::shortcuts::VendorNotification>
VendorEventDecoder::consume(std::span<const std::uint8_t> packet, std::chrono::milliseconds observed_at) {
    if (!supported_ || !valid_time(observed_at) || !recognized(packet))
        return {};
    if (packet[1] != 0x18 && packet[1] != 0x19)
        return announcement(packet);
    if (packet[2] != 1)
        return {};
    if (packet[1] == 0x18) {
        if (!leading_ || observed_at - *leading_ > pair_window)
            leading_ = observed_at;
        return {};
    }
    return finish_activation(observed_at);
}

} // namespace manager::adapters::windows_shortcuts

namespace manager::adapters::windows_shortcuts {
std::optional<application::shortcuts::VendorNotification>
VendorEventDecoder::finish_activation(std::chrono::milliseconds observed_at) {
    const auto leading = leading_;
    leading_.reset();
    if (!leading || observed_at - *leading > pair_window)
        return {};
    return application::shortcuts::VendorActivation{application::shortcuts::VendorKey::MiButton, ++sequence_};
}
} // namespace manager::adapters::windows_shortcuts
