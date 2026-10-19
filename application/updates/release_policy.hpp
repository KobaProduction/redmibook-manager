#pragma once
#include <array>
#include <optional>
#include <string>
#include <string_view>

namespace manager::application {
using ReleaseVersion = std::array<unsigned, 3>;
std::optional<ReleaseVersion> release_version(std::string_view text);
std::string package_asset(const ReleaseVersion &version, std::string_view target);
} // namespace manager::application
