#pragma once
#include <array>
#include <cstdint>
#include <optional>

namespace manager::application::feedback {
enum class Kind : std::uint8_t { Microphone, Performance, Refresh };
struct Event {
    Kind kind;
    unsigned int value;
};
enum class Delivery : std::uint8_t { Queued, Presented, Suppressed, Unavailable, Invalid };
constexpr bool valid(Event event) {
    switch (event.kind) {
    case Kind::Microphone:
        return event.value <= 1;
    case Kind::Performance:
        return event.value >= 1 && event.value <= 3;
    case Kind::Refresh:
        return event.value >= 2 && event.value <= 1000;
    }
    return false;
}
class Presenter {
  public:
    virtual ~Presenter() = default;
    // Nonblocking. Queued admits delivery, not visible rendering. No hardware commands.
    virtual Delivery present(Event event) = 0;
};
// OEM announcements and confirmed GUI results can describe the same transition.
// Preserve per-kind ordering without time-based retries or replay after reconnect.
class DistinctState final {
  public:
    bool accept(Event event) {
        if (!valid(event))
            return false;
        auto &previous = values_.at(static_cast<std::size_t>(event.kind));
        if (previous == event.value)
            return false;
        previous = event.value;
        return true;
    }
    void forget(Kind kind) {
        values_.at(static_cast<std::size_t>(kind)).reset();
    }

  private:
    std::array<std::optional<unsigned int>, 3> values_;
};
} // namespace manager::application::feedback
