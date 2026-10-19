#pragma once
#include <filesystem>
#include <set>

namespace manager::adapters::windows {
using PayloadManifest = std::set<std::filesystem::path>;
[[nodiscard]] PayloadManifest read_manifest(const std::filesystem::path &root);
[[nodiscard]] bool prune_obsolete(const std::filesystem::path &root,
                                  const PayloadManifest &previous) noexcept;
} // namespace manager::adapters::windows
