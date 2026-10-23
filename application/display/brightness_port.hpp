#pragma once
#include <cstdint>
#include <optional>

namespace manager::application::display {
enum class BrightnessStatus : std::uint8_t {
    Ready,
    NotImplemented,
    Unavailable,
    Denied,
    Failed,
    Unconfirmed
};
struct Brightness {
    BrightnessStatus status{BrightnessStatus::Unavailable};
    std::optional<unsigned int> percent;
};
class BrightnessPort {
  public:
    virtual ~BrightnessPort() = default;
    virtual Brightness read() const = 0;
    virtual Brightness set(unsigned int percent) = 0;
};
} // namespace manager::application::display
