#include "application/microphone/synchronize.hpp"
#include <cstdlib>

namespace {
using namespace manager::application::microphone;
class Fake final : public ControlPort {
  public:
    mutable int writes{};
    Observation initial;
    std::optional<Observation> response;
    std::function<void()> on_read;
    Observation read() const override {
        if (on_read)
            on_read();
        return initial;
    }
    Observation set_muted(bool value) const override {
        ++writes;
        return response.value_or(Observation{Status::Ready, value, {}});
    }
};
class Firmware final : public FirmwareStatePort {
  public:
    FirmwareState state{FirmwareStateStatus::Ready, true, {}};
    FirmwareState read() const override {
        return state;
    }
};
bool synchronization() {
    const auto current_intent = [] { return true; };
    Firmware firmware;
    Fake endpoint;
    endpoint.initial = {Status::Ready, false, {}};
    if (synchronize(firmware, endpoint, current_intent).muted != true || endpoint.writes != 1)
        return false;
    endpoint.initial.muted = true;
    synchronize(firmware, endpoint, current_intent);
    if (endpoint.writes != 1)
        return false;
    firmware.state = {FirmwareStateStatus::Denied, {}, {}};
    if (synchronize(firmware, endpoint, current_intent).status == Status::Ready || endpoint.writes != 1)
        return false;
    firmware.state = {FirmwareStateStatus::Ready, false, {}};
    return synchronize(firmware, endpoint, current_intent).muted == false && endpoint.writes == 2;
}
bool superseded_unmute() {
    Firmware firmware;
    firmware.state.desired_muted = false;
    Fake endpoint;
    endpoint.initial = {Status::Ready, true, {}};
    bool current = true;
    endpoint.on_read = [&] {
        firmware.state.desired_muted = true;
        current = false;
    };
    const auto result = synchronize(firmware, endpoint, [&] { return current; });
    return result.status == Status::Failed && !result.muted && endpoint.writes == 0;
}
bool confirmed_writes() {
    Fake control;
    control.initial = {Status::Ready, true, {}};
    if (toggle(control).muted != false || control.writes != 1)
        return false;
    control.response = Observation{Status::Ready, false, {}};
    if (set_muted_confirmed(control, true).status != Status::Failed)
        return false;
    control.response = Observation{Status::Ready, {}, {}};
    if (set_muted_confirmed(control, false).status != Status::Failed)
        return false;
    control.response = Observation{Status::Denied, {}, {}};
    return set_muted_confirmed(control, true).status == Status::Denied;
}
} // namespace
int main() {
    Fake control;
    control.initial = {Status::Denied, {}, {}};
    if (toggle(control).status != Status::Denied || control.writes) {
        return EXIT_FAILURE;
    }
    control.initial = {Status::Ready, {}, {}};
    if (toggle(control).muted || control.writes) {
        return EXIT_FAILURE;
    }
    control.initial = {Status::Ready, false, {}};
    if (toggle(control).muted != true || control.writes != 1) {
        return EXIT_FAILURE;
    }
    return confirmed_writes() && synchronization() && superseded_unmute() ? EXIT_SUCCESS : EXIT_FAILURE;
}
