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
};
class ControlPort {
  public:
    virtual ~ControlPort() = default;
    virtual Observation read() const = 0;
    virtual Observation set_muted(bool muted) const = 0;
};
inline Observation toggle(const ControlPort &control) {
    auto before = control.read();
    if (before.status != Status::Ready || !before.muted) {
        return before;
    }
    return control.set_muted(!*before.muted);
}
} // namespace manager::application::microphone
