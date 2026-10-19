#include "adapters/windows/hosting/payload_manifest.hpp"
#include <chrono>
#include <filesystem>
#include <fstream>

namespace {
class Fixture final {
  public:
    const std::filesystem::path temporary{std::filesystem::temp_directory_path()};
    const std::filesystem::path root{
        temporary /
        ("rmb-manifest-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()))};
    Fixture() {
        std::filesystem::create_directory(root);
    }
    ~Fixture() {
        if (root.parent_path() == temporary && root.filename().string().starts_with("rmb-manifest-")) {
            std::filesystem::remove_all(root);
        }
    }
    Fixture(const Fixture &) = delete;
    Fixture &operator=(const Fixture &) = delete;
};

bool check_pruning(const std::filesystem::path &root) {
    std::ofstream(root / "keep.txt") << "new";
    std::ofstream(root / "old.txt") << "old";
    std::ofstream(root / "unowned.txt") << "user";
    std::ofstream(root / "payload.manifest") << "keep.txt\npayload.manifest\n";
    const manager::adapters::windows::PayloadManifest previous{"keep.txt", "old.txt"};
    if (!manager::adapters::windows::prune_obsolete(root, previous)) {
        return false;
    }
    return std::filesystem::exists(root / "keep.txt") && std::filesystem::exists(root / "unowned.txt") &&
           !std::filesystem::exists(root / "old.txt");
}

bool rejects_traversal(const std::filesystem::path &root) {
    std::ofstream(root / "payload.manifest") << "../outside.txt\n";
    try {
        const auto ignored = manager::adapters::windows::read_manifest(root);
    } catch (const std::exception &) {
        return true;
    }
    return false;
}
} // namespace

int main() {
    const Fixture fixture;
    return check_pruning(fixture.root) && rejects_traversal(fixture.root) ? 0 : 1;
}
