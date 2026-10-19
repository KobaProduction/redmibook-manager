#pragma once
#include <cstdint>
#include <functional>
#include <string>

namespace manager::application {
enum class UpdateState : std::uint8_t { Current, Available, Unavailable, Failed };
struct UpdateResult {
    UpdateState state{UpdateState::Unavailable};
    std::string version{};
    std::string release_url{};
    std::string download_url{};
};
class UpdatePort {
  public:
    virtual ~UpdatePort() = default;
    virtual void check(std::function<void(UpdateResult)> completion) = 0;
};
} // namespace manager::application
