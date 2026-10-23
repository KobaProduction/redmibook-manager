#pragma once
#include <cstdint>
#include <optional>
#include <string>

namespace manager::application::microphone {
enum class Status : std::uint8_t { Ready, NotImplemented, Unavailable, Denied, Failed };
struct Observation {
    Status status{Status::Unavailable};
    std::optional<bool> muted;
    std::string detail;
    // Driver-reported endpoint capability; this is not the current mute state.
    std::optional<bool> hardware_mute_supported{};
    bool operator==(const Observation &) const = default;
};
class ControlPort {
  public:
    virtual ~ControlPort() = default;
    virtual Observation read() const = 0;
    virtual Observation set_muted(bool muted) const = 0;
};
inline Observation set_muted_confirmed(const ControlPort &control, bool muted) {
    auto observed = control.set_muted(muted);
    if (observed.status == Status::Ready && observed.muted != muted) {
        observed.status = Status::Failed;
        observed.detail = "microphone-readback-mismatch";
    }
    return observed;
}
inline Observation toggle(const ControlPort &control) {
    auto before = control.read();
    if (before.status != Status::Ready || !before.muted) {
        return before;
    }
    return set_muted_confirmed(control, !*before.muted);
}
} // namespace manager::application::microphone
