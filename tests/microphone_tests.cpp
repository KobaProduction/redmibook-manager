#include "application/microphone/control_port.hpp"
#include <cstdlib>

namespace {
using namespace manager::application::microphone;
class Fake final : public ControlPort {
  public:
    mutable int writes{};
    Observation initial;
    Observation read() const override {
        return initial;
    }
    Observation set_muted(bool value) const override {
        ++writes;
        return {Status::Ready, value, {}};
    }
};
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
    return EXIT_SUCCESS;
}
