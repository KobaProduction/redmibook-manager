#pragma once

#include <cstdint>
#include <variant>

namespace manager::application::shortcuts {

enum class VendorKey : std::uint8_t { MiButton };

// One normalized activation from a qualified device-event sequence, not a raw
// key-down/key-up claim. The application chooses the configured action separately.
struct VendorActivation {
    VendorKey key{VendorKey::MiButton};
    std::uint64_t sequence{};
};

// Firmware announcements are observations of a requested/reported vendor state.
// They must not be turned into duplicate firmware SETs or UI success claims.
struct FirmwarePerformanceAnnouncement {
    std::uint8_t announced_profile{};
};

struct RefreshKeyNotification {};

struct FirmwareMicrophoneDesiredState {
    bool muted{};
};

using VendorNotification = std::variant<VendorActivation, FirmwarePerformanceAnnouncement,
                                        RefreshKeyNotification, FirmwareMicrophoneDesiredState>;

} // namespace manager::application::shortcuts
