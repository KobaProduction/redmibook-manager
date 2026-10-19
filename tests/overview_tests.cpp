#include "application/overview/read_overview.hpp"

#include <iostream>

namespace {
class FakeHost final : public manager::application::HostInfoPort {
  public:
    explicit FakeHost(manager::domain::HostPlatform platform) : platform_(platform) {}
    manager::domain::HostPlatform platform() const override {
        return platform_;
    }

  private:
    manager::domain::HostPlatform platform_;
};

bool check_platform(manager::domain::HostPlatform platform) {
    const FakeHost host(platform);
    const manager::application::ReadOverview query(host);
    const auto overview = query.execute();
    return overview.platform == platform && overview.greeting == "Hello, World!";
}
} // namespace

int main() {
    if (!check_platform(manager::domain::HostPlatform::Windows) ||
        !check_platform(manager::domain::HostPlatform::Linux)) {
        std::cerr << "Overview must preserve the host platform and greeting\n";
        return 1;
    }
    return 0;
}
