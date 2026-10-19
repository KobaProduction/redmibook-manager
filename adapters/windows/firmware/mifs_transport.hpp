#pragma once

#include <array>
#include <cstdint>
#include <string>

namespace manager::adapters::windows_firmware {

struct FirmwareIdentity {
    std::string manufacturer;
    std::string board;
    std::string bios;
};

enum class FirmwareSupport : std::uint8_t { UnsupportedHardware, UnverifiedFirmware, VerifiedProtocol };
[[nodiscard]] FirmwareSupport firmware_support(const FirmwareIdentity &identity);

enum class MifsCommand : std::uint8_t {
    ReadCharge,
    Protect80,
    FullCharge,
    ReadPerformance,
    Balanced,
    Quiet,
    Turbo,
    FullSpeed
};
enum class TransportStatus : std::uint8_t {
    Success,
    PermissionDenied,
    Unavailable,
    Unsupported,
    Failed,
    OutcomeUnknown
};

struct MifsReply {
    TransportStatus status{TransportStatus::Unavailable};
    std::array<std::uint8_t, 32> bytes{};
    std::string detail;
};

// Narrow command vocabulary; no arbitrary WMI method, selector, EC or firmware access.
class MifsTransport {
  public:
    virtual ~MifsTransport() = default;
    [[nodiscard]] virtual MifsReply exchange(MifsCommand command) = 0;
};

[[nodiscard]] std::array<std::uint8_t, 32> encode(MifsCommand command);
[[nodiscard]] MifsReply validate_reply(MifsCommand command, MifsReply reply);
[[nodiscard]] std::uint16_t word(const MifsReply &reply, std::size_t offset);
[[nodiscard]] std::uint32_t dword(const MifsReply &reply, std::size_t offset);

class NativeWmiTransport final : public MifsTransport {
  public:
    [[nodiscard]] MifsReply exchange(MifsCommand command) override;
};

} // namespace manager::adapters::windows_firmware
