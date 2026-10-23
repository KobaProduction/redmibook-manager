#include "adapters/linux/battery/charge_control.hpp"
#include "adapters/linux/performance/performance_control.hpp"
#include "adapters/windows/battery/charge_control.hpp"
#include "adapters/windows/performance/performance_control.hpp"

#include <bit>
#include <iostream>
#include <vector>

namespace {

namespace battery = manager::application::battery;
namespace firmware = manager::adapters::windows_firmware;
namespace performance = manager::application::performance;

class FakeMifs final : public firmware::MifsTransport {
  public:
    std::vector<firmware::MifsCommand> calls;
    bool accept{true};
    bool match{true};
    std::uint8_t charge{1};
    std::uint8_t profile{2};
    firmware::TransportStatus transport_status{firmware::TransportStatus::Success};

    firmware::MifsReply exchange(firmware::MifsCommand command) override {
        calls.push_back(command);
        if (transport_status != firmware::TransportStatus::Success)
            return {transport_status, {}, "Injected transport failure"};
        firmware::MifsReply reply{firmware::TransportStatus::Success, {}, {}};
        reply.bytes[1] = accept ? 0x80 : 0xE0;
        if (command == firmware::MifsCommand::ReadCharge) {
            reply.bytes[3] = 0x10;
            reply.bytes[4] = 2;
            reply.bytes[6] = charge;
        } else if (command == firmware::MifsCommand::ReadPerformance) {
            reply.bytes[3] = 0x08;
            reply.bytes[4] = profile;
        } else if (match) {
            apply(command);
        }
        return reply;
    }

  private:
    void apply(firmware::MifsCommand command) {
        if (command == firmware::MifsCommand::Protect80)
            charge = 1;
        else if (command == firmware::MifsCommand::FullCharge)
            charge = 0;
        else
            profile = firmware::encode(command)[4];
    }
};

bool protocol_contract() {
    const auto protect = firmware::encode(firmware::MifsCommand::Protect80);
    const auto full = firmware::encode(firmware::MifsCommand::FullCharge);
    const auto get = firmware::encode(firmware::MifsCommand::ReadCharge);
    if (protect[1] != 0xFB || protect[3] != 0x10 || protect[4] != 2 || protect[6] != 1)
        return false;
    if (full[6] != 0 || get[1] != 0xFA)
        return false;
    firmware::MifsReply wrong{firmware::TransportStatus::Success, {}, {}};
    wrong.bytes[1] = 0x80;
    return firmware::validate_reply(firmware::MifsCommand::ReadCharge, wrong).status ==
               firmware::TransportStatus::Failed &&
           firmware::validate_reply(firmware::MifsCommand::Protect80, wrong).status ==
               firmware::TransportStatus::Success;
}

bool unverified_never_writes() {
    FakeMifs transport;
    manager::adapters::windows_battery::ChargeControl control(transport,
                                                              {"XIAOMI", "TM2309", "RMAMT6B0UNKNOWN"});
    const auto result = battery::apply_charge_limit(control, battery::ChargeLimit::Protection80);
    if (result.status != battery::Status::UnverifiedFirmware || !transport.calls.empty())
        return false;
    return control.read().limit == battery::ChargeLimit::Protection80 && transport.calls.size() == 1;
}

bool confirmed_readback() {
    FakeMifs transport;
    manager::adapters::windows_battery::ChargeControl control(transport,
                                                              {"XIAOMI", "TM2309", "RMAMT6B0P0A0A"});
    if (battery::apply_charge_limit(control, battery::ChargeLimit::Full).status != battery::Status::Confirmed)
        return false;
    transport.match = false;
    if (battery::apply_charge_limit(control, battery::ChargeLimit::Protection80).status !=
        battery::Status::AppliedUnconfirmed)
        return false;
    transport.accept = false;
    return battery::apply_charge_limit(control, battery::ChargeLimit::Protection80).status ==
           battery::Status::UnsupportedHardware;
}

bool unsupported_and_malformed() {
    FakeMifs transport;
    manager::adapters::windows_battery::ChargeControl foreign(transport,
                                                              {"OTHER", "TM2309", "RMAMT6B0P0B0B"});
    if (foreign.read().status != battery::Status::UnsupportedHardware || !transport.calls.empty())
        return false;
    manager::adapters::windows_battery::ChargeControl control(transport,
                                                              {"XIAOMI", "TM2309", "RMAMT6B0P0B0B"});
    if (battery::apply_charge_limit(control, std::bit_cast<battery::ChargeLimit>(std::uint8_t{40})).status !=
        battery::Status::InvalidRequest)
        return false;
    transport.charge = 8;
    return control.read().status == battery::Status::Failed;
}

bool performance_readback() {
    FakeMifs transport;
    manager::adapters::windows_performance::PerformanceControl control(transport,
                                                                       {"XIAOMI", "TM2309", "RMAMT6B0P0A0A"});
    const auto result = performance::apply_profile(control, performance::Profile::Balanced);
    if (result.status != performance::Status::Confirmed || result.profile != performance::Profile::Balanced)
        return false;
    const auto previous_calls = transport.calls.size();
    if (performance::apply_profile(control, performance::Profile::FullSpeed).status !=
            performance::Status::InvalidRequest ||
        transport.calls.size() != previous_calls)
        return false;
    transport.profile = 9;
    return control.read().status == performance::Status::Failed;
}

bool pending_write_never_confirms() {
    FakeMifs transport;
    transport.transport_status = firmware::TransportStatus::OutcomeUnknown;
    manager::adapters::windows_battery::ChargeControl control(transport,
                                                              {"XIAOMI", "TM2309", "RMAMT6B0P0B0B"});
    const auto result = battery::apply_charge_limit(control, battery::ChargeLimit::Protection80);
    return result.status == battery::Status::OutcomeUnknown && !result.limit && transport.calls.size() == 1;
}

bool permission_denial_is_not_success() {
    FakeMifs transport;
    transport.transport_status = firmware::TransportStatus::PermissionDenied;
    manager::adapters::windows_battery::ChargeControl control(transport,
                                                              {"XIAOMI", "TM2309", "RMAMT6B0P0B0B"});
    const auto result = battery::apply_charge_limit(control, battery::ChargeLimit::Protection80);
    return result.status == battery::Status::PermissionDenied && !result.limit && transport.calls.size() == 1;
}

bool linux_is_unimplemented() {
    manager::adapters::linux_battery::ChargeControl battery_control;
    manager::adapters::linux_performance::PerformanceControl performance_control;
    return battery_control.read().status == battery::Status::NotImplemented &&
           battery::apply_charge_limit(battery_control, battery::ChargeLimit::Full).status ==
               battery::Status::NotImplemented &&
           performance_control.read().status == performance::Status::NotImplemented;
}

} // namespace

int main() {
    if (!protocol_contract() || !unverified_never_writes() || !confirmed_readback() ||
        !unsupported_and_malformed() || !performance_readback() || !permission_denial_is_not_success() ||
        !linux_is_unimplemented() || !pending_write_never_confirms()) {
        std::cerr << "Battery/performance policy contract failed\n";
        return 1;
    }
    return 0;
}
