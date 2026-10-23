#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace manager::application::display {
enum class Status : std::uint8_t {
    Ready,
    NotImplemented,
    Unavailable,
    InvalidRequest,
    Denied,
    Failed,
    Unconfirmed
};
struct Display {
    std::string id;
    std::string name;
    unsigned int width{};
    unsigned int height{};
    unsigned int current_hz{};
    std::vector<unsigned int> available_hz;
    // Positively identified internal panel with no cloned targets sharing its source.
    // False includes unknown topology; callers must not fall back to the primary display.
    bool internal_panel{false};
    bool operator==(const Display &) const = default;
};
struct Snapshot {
    Status status{Status::Unavailable};
    std::vector<Display> displays;
    std::string detail;
    bool operator==(const Snapshot &) const = default;
};
struct Change {
    Status status{Status::Unavailable};
    unsigned int observed_hz{};
    std::string detail;
};
class RefreshPort {
  public:
    virtual ~RefreshPort() = default;
    virtual Snapshot read() const = 0;
    virtual Change set_refresh(const std::string &id, unsigned int hz) const = 0;
};
} // namespace manager::application::display
