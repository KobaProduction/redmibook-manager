#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace manager::application::power {
enum class Status : std::uint8_t { Ready, NotImplemented, Unavailable, Denied, Failed, Unconfirmed };
struct Scheme {
    std::string id;
    std::string name;
};
struct Snapshot {
    Status status{Status::Unavailable};
    std::vector<Scheme> schemes{};
    std::string active;
};
class SchemePort {
  public:
    virtual ~SchemePort() = default;
    virtual Snapshot read() const = 0;
    virtual Status select(const std::string &id) = 0;
};
} // namespace manager::application::power
