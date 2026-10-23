#include "adapters/platform/power_scheme.hpp"
#include <iostream>
int main() {
    const auto result = manager::adapters::make_power_schemes()->read();
    std::cout << "status=" << static_cast<int>(result.status) << " active=" << result.active << '\n';
    for (const auto &scheme : result.schemes)
        std::cout << scheme.id << ' ' << scheme.name << '\n';
    return result.status == manager::application::power::Status::Ready ? 0 : 1;
}
