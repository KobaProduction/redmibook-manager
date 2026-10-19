#pragma once
#include <filesystem>
#include <string_view>

namespace manager::adapters::windows {
int service_command(std::string_view command);
[[nodiscard]] bool service_running();
struct MaintenanceRequest {
    std::filesystem::path directory;
    std::filesystem::path marker;
};
int maintain_installation(const MaintenanceRequest &request);
} // namespace manager::adapters::windows
