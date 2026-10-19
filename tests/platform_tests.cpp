#include "adapters/platform/host_info.hpp"

#include <iostream>
#include <string_view>

int main(int argc, char *argv[]) {
    if (argc != 2) {
        std::cerr << "Expected the CMake-selected host platform\n";
        return 1;
    }
    const manager::adapters::HostInfo host;
    const auto expected = host.platform() == manager::domain::HostPlatform::Windows ? "windows" : "linux";
    if (std::string_view(argv[1]) != expected) {
        std::cerr << "The native adapter does not match the selected platform\n";
        return 1;
    }
    return 0;
}
