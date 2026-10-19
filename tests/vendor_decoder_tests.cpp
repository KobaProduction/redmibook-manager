#include "adapters/windows/shortcuts/vendor_decoder.hpp"

#include <array>
#include <iostream>

namespace {

using manager::adapters::windows_shortcuts::VendorEventDecoder;
using namespace std::chrono_literals;

manager::application::DeviceProfile target() {
    manager::application::DeviceProfile profile;
    profile.evidence = manager::application::ProfileEvidence::Observed;
    profile.status = manager::application::ProfileReadStatus::Available;
    profile.manufacturer = "XIAOMI";
    profile.board_model = "TM2309";
    profile.bios_version = "RMAMT6B0P0A0A";
    return profile;
}

std::array<std::uint8_t, 32> packet(std::uint8_t code) {
    std::array<std::uint8_t, 32> bytes{};
    bytes[0] = 1;
    bytes[1] = code;
    bytes[2] = 1;
    return bytes;
}

std::optional<manager::application::shortcuts::VendorActivation>
as_activation(const std::optional<manager::application::shortcuts::VendorNotification> &notification) {
    if (!notification)
        return {};
    const auto *activation = std::get_if<manager::application::shortcuts::VendorActivation>(&*notification);
    return activation ? std::optional(*activation) : std::nullopt;
}

bool captured_pairs() {
    VendorEventDecoder decoder(target());
    const std::array leading{13432ms, 14028ms, 14614ms};
    const std::array trailing{13491ms, 14083ms, 14704ms};
    for (std::size_t index = 0; index < leading.size(); ++index) {
        if (decoder.consume(packet(0x18), leading[index]))
            return false;
        const auto activated = as_activation(decoder.consume(packet(0x19), trailing[index]));
        if (!activated || activated->sequence != index + 1)
            return false;
    }
    return true;
}

bool duplicate_edges_and_orphans() {
    VendorEventDecoder decoder(target());
    if (decoder.consume(packet(0x19), 0ms) || decoder.consume(packet(0x18), 10ms) ||
        decoder.consume(packet(0x18), 20ms))
        return false;
    const auto first = as_activation(decoder.consume(packet(0x19), 70ms));
    if (!first || first->sequence != 1 || decoder.consume(packet(0x19), 80ms))
        return false;
    decoder.reset();
    return !decoder.consume(packet(0x19), 90ms);
}

bool exact_firmware_gate() {
    auto profile = target();
    profile.bios_version = "RMAMT6B0P0B0B";
    VendorEventDecoder other_bios(profile);
    profile = target();
    profile.board_model = "TM2424";
    VendorEventDecoder other_board(profile);
    profile = target();
    profile.evidence = manager::application::ProfileEvidence::Demo;
    VendorEventDecoder demo(profile);
    return !other_bios.supported() && !other_board.supported() && !demo.supported() &&
           !other_bios.consume(packet(0x18), 0ms) && !other_bios.consume(packet(0x19), 60ms);
}

bool invalid_packets() {
    VendorEventDecoder decoder(target());
    auto wrong_tail = packet(0x18);
    wrong_tail[31] = 1;
    const std::array<std::uint8_t, 3> short_packet{1, 0x18, 1};
    return !decoder.consume(wrong_tail, 0ms) && !decoder.consume(short_packet, 10ms) &&
           !decoder.consume(packet(0x25), 20ms) && !decoder.consume(packet(0x19), 80ms);
}

bool stale_and_reordered() {
    VendorEventDecoder decoder(target());
    if (decoder.consume(packet(0x18), 0ms) || decoder.consume(packet(0x19), 1001ms) ||
        decoder.consume(packet(0x18), 2000ms) || decoder.consume(packet(0x19), 1999ms) ||
        decoder.consume(packet(0x19), 2020ms))
        return false;
    if (decoder.consume(packet(0x18), 3000ms))
        return false;
    const auto activated = as_activation(decoder.consume(packet(0x19), 3060ms));
    return activated && activated->sequence == 1;
}

bool original_key_notifications() {
    namespace shortcuts = manager::application::shortcuts;
    VendorEventDecoder decoder(target());
    auto performance = packet(0x16);
    for (std::uint8_t profile = 1; profile <= 4; ++profile) {
        performance[2] = profile;
        const auto notified = decoder.consume(performance, 16000ms + std::chrono::milliseconds(profile));
        const auto *announced =
            notified ? std::get_if<shortcuts::FirmwarePerformanceAnnouncement>(&*notified) : nullptr;
        if (profile == 4 ? notified.has_value() : (!announced || announced->announced_profile != profile))
            return false;
    }
    auto refresh = packet(0x1A);
    refresh[2] = 0;
    const auto requested = decoder.consume(refresh, 22256ms);
    return requested && std::holds_alternative<shortcuts::RefreshKeyNotification>(*requested);
}

bool microphone_is_desired_state_only() {
    namespace shortcuts = manager::application::shortcuts;
    VendorEventDecoder decoder(target());
    auto muted_packet = packet(0x21);
    muted_packet[2] = 0;
    const auto muted = decoder.consume(muted_packet, 32733ms);
    const auto unmuted = decoder.consume(packet(0x21), 39297ms);
    const auto *desired_mute =
        muted ? std::get_if<shortcuts::FirmwareMicrophoneDesiredState>(&*muted) : nullptr;
    const auto *desired_unmute =
        unmuted ? std::get_if<shortcuts::FirmwareMicrophoneDesiredState>(&*unmuted) : nullptr;
    return desired_mute && desired_mute->muted && desired_unmute && !desired_unmute->muted;
}

} // namespace

int main() {
    if (!captured_pairs() || !duplicate_edges_and_orphans() || !exact_firmware_gate() || !invalid_packets() ||
        !stale_and_reordered() || !original_key_notifications() || !microphone_is_desired_state_only()) {
        std::cerr << "Exact TM2309 Mi-button decoder contract failed\n";
        return 1;
    }
    return 0;
}
