#pragma once
#include "application/updates/update_port.hpp"
#include <cstdint>
#include <functional>

namespace manager::application {
enum class InstallResult : std::uint8_t { Started, Cancelled, Failed };
class UpdatePackagePort {
  public:
    // Asynchronous operations. Deliver callbacks on the caller's event context.
    // Successful download means a complete, verified package; Started means launch,
    // never proof that installation completed.
    using Progress = std::function<void(std::uint64_t, std::uint64_t)>;
    virtual ~UpdatePackagePort() = default;
    virtual void download(const UpdateResult &release, Progress progress,
                          std::function<void(bool)> completion) = 0;
    virtual void install(std::function<void(InstallResult)> completion) = 0;
};
} // namespace manager::application
