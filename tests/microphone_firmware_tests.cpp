#include "adapters/windows/firmware/microphone_state.hpp"
#include <iostream>
namespace {
namespace fw = manager::adapters::windows_firmware;
namespace mic = manager::application::microphone;
class Transport final : public fw::MifsTransport {
  public:
    fw::MifsReply reply{fw::TransportStatus::Success, {}, {}};
    int reads{};
    int writes{};
    bool ignore_write{false};
    fw::TransportStatus write_status{fw::TransportStatus::Success};
    Transport() {
        reply.bytes[1] = 0x80;
        reply.bytes[3] = 0x0A;
        reply.bytes[4] = 5;
    }
    fw::MifsReply exchange(fw::MifsCommand command) override {
        if (command == fw::MifsCommand::ReadMicrophone)
            ++reads;
        else {
            ++writes;
            if (write_status != fw::TransportStatus::Success)
                return {write_status, {}, "injected-write-failure"};
            if (!ignore_write)
                reply.bytes[6] = command == fw::MifsCommand::UnmuteMicrophone ? 1 : 0;
        }
        return reply;
    }
};
constexpr const char *bios = "RMAMT6B0P0A0A";
bool packet_and_polarity() {
    std::array<std::uint8_t, 32> expected{};
    expected[1] = 0xFA;
    expected[3] = 0x0A;
    expected[4] = 5;
    if (fw::encode(fw::MifsCommand::ReadMicrophone) != expected)
        return false;
    Transport transport;
    const auto muted = fw::read_microphone_state(transport, {"XIAOMI", "TM2309", bios});
    transport.reply.bytes[6] = 1;
    const auto active = fw::read_microphone_state(transport, {"XIAOMI", "TM2309", bios});
    return muted.status == mic::FirmwareStateStatus::Ready && muted.desired_muted == true &&
           active.status == mic::FirmwareStateStatus::Ready && active.desired_muted == false &&
           transport.reads == 2;
}
bool writes_and_readback() {
    Transport transport;
    const fw::FirmwareIdentity identity{"XIAOMI", "TM2309", bios};
    if (fw::request_microphone_state(transport, identity, false).desired_muted != false)
        return false;
    if (fw::request_microphone_state(transport, identity, true).desired_muted != true)
        return false;
    transport.ignore_write = true;
    if (fw::request_microphone_state(transport, identity, false).status != mic::FirmwareStateStatus::Failed)
        return false;
    transport.write_status = fw::TransportStatus::OutcomeUnknown;
    if (fw::request_microphone_state(transport, identity, false).status !=
        mic::FirmwareStateStatus::OutcomeUnknown)
        return false;
    const int count = transport.writes;
    fw::request_microphone_state(transport, {"XIAOMI", "TM2309", "other"}, false);
    return transport.writes == count;
}
bool write_packets() {
    auto expected = fw::encode(fw::MifsCommand::ReadMicrophone);
    expected[1] = 0xFB;
    if (fw::encode(fw::MifsCommand::MuteMicrophone) != expected)
        return false;
    expected[6] = 1;
    return fw::encode(fw::MifsCommand::UnmuteMicrophone) == expected;
}
bool invalid_replies() {
    for (const auto offset : {1, 3, 4, 6, 7}) {
        Transport transport;
        transport.reply.bytes[static_cast<std::size_t>(offset)] = 7;
        const auto state = fw::read_microphone_state(transport, {"XIAOMI", "TM2309", bios});
        if (state.status == mic::FirmwareStateStatus::Ready || state.desired_muted)
            return false;
    }
    Transport denied;
    denied.reply.status = fw::TransportStatus::PermissionDenied;
    const auto state = fw::read_microphone_state(denied, {"XIAOMI", "TM2309", bios});
    return state.status == mic::FirmwareStateStatus::Denied && !state.desired_muted;
}
bool qualification() {
    Transport transport;
    const auto other = fw::read_microphone_state(transport, {"OTHER", "TM2309", bios});
    const auto unqualified = fw::read_microphone_state(transport, {"XIAOMI", "TM2309", "RMAMT6B0P0B0B"});
    bool unknown = false;
    static_cast<void>(fw::observe_reply(fw::MifsCommand::ReadMicrophone,
                                        {fw::TransportStatus::OutcomeUnknown, {}, {}}, unknown));
    return other.status == mic::FirmwareStateStatus::UnsupportedHardware &&
           unqualified.status == mic::FirmwareStateStatus::UnverifiedFirmware && transport.reads == 0 &&
           !unknown;
}
} // namespace
int main() {
    const bool passed = packet_and_polarity() && invalid_replies() && qualification() &&
                        writes_and_readback() && write_packets();
    if (!passed)
        std::cerr << "Firmware microphone GET contract failed\n";
    return passed ? 0 : 1;
}
