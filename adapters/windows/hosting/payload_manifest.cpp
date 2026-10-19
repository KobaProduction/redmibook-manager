#include "adapters/windows/hosting/payload_manifest.hpp"
#include <algorithm>
#include <fstream>
#include <stdexcept>
#include <string>

namespace manager::adapters::windows {
namespace {
bool safe_relative(const std::filesystem::path &path) {
    if (path.empty() || path.is_absolute() || path.has_root_name()) {
        return false;
    }
    return std::none_of(path.begin(), path.end(), [](const auto &part) { return part == ".."; });
}

bool inside_root(const std::filesystem::path &root, const std::filesystem::path &target) {
    const auto canonical_root = std::filesystem::weakly_canonical(root);
    const auto canonical_target = std::filesystem::weakly_canonical(target);
    return std::mismatch(canonical_root.begin(), canonical_root.end(), canonical_target.begin(),
                         canonical_target.end())
               .first == canonical_root.end();
}
} // namespace

PayloadManifest read_manifest(const std::filesystem::path &root) {
    std::ifstream input(root / "payload.manifest");
    PayloadManifest result;
    std::string line;
    while (std::getline(input, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        const std::filesystem::path relative(std::u8string(line.begin(), line.end()));
        if (!safe_relative(relative) || result.size() >= 10000) {
            throw std::runtime_error("Invalid payload manifest");
        }
        result.insert(relative.lexically_normal());
    }
    return result;
}

bool prune_obsolete(const std::filesystem::path &root, const PayloadManifest &previous) noexcept {
    try {
        if (!std::filesystem::is_regular_file(root / "payload.manifest")) {
            return false;
        }
        const auto current = read_manifest(root);
        for (const auto &relative : previous) {
            if (current.contains(relative)) {
                continue;
            }
            const auto target = root / relative;
            if (!inside_root(root, target)) {
                return false;
            }
            if (std::filesystem::is_regular_file(target)) {
                std::filesystem::remove(target);
            }
        }
        return true;
    } catch (const std::exception &) {
        return false;
    }
}
} // namespace manager::adapters::windows
