#pragma once
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>

namespace manager::application::power {
enum class SaverStatus : std::uint8_t { Ready, NotImplemented, Unavailable, Denied, Failed, Unconfirmed };
struct Saver {
    SaverStatus status{SaverStatus::Unavailable};
    std::optional<bool> enabled;
    bool controllable{false};
};
class SaverObservation {
  public:
    virtual ~SaverObservation() = default;
};
class SaverPort {
  public:
    virtual ~SaverPort() = default;
    virtual Saver read() const = 0;
    // Periodic reconciliation must fetch authoritative state, not just a notification cache.
    virtual Saver reconcile() const {
        return read();
    }
    virtual Saver set(bool enabled) = 0;
    // Destruction must detach the listener and drain callbacks already in progress.
    // Notification only schedules a read; it never executes a hardware command.
    virtual std::unique_ptr<SaverObservation> observe(std::function<void()> changed) = 0;
};
} // namespace manager::application::power
