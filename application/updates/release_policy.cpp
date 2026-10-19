#include "application/updates/release_policy.hpp"
#include <charconv>

namespace manager::application {
namespace {
bool parse_component(std::string_view text, unsigned &value) {
    const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value);
    return !text.empty() && parsed.ec == std::errc{} && parsed.ptr == text.data() + text.size();
}
} // namespace

std::optional<ReleaseVersion> release_version(std::string_view text) {
    if (text.starts_with('v')) {
        text.remove_prefix(1);
    }
    ReleaseVersion result{};
    for (auto &part : result) {
        const auto dot = text.find('.');
        const auto segment = text.substr(0, dot);
        if (&part == &result.back() && dot != std::string_view::npos) {
            return std::nullopt;
        }
        if (!parse_component(segment, part)) {
            return std::nullopt;
        }
        text = dot == std::string_view::npos ? std::string_view{} : text.substr(dot + 1);
        if (&part != &result.back() && dot == std::string_view::npos) {
            return std::nullopt;
        }
    }
    return result;
}
std::string package_asset(const ReleaseVersion &version, std::string_view target) {
    const auto prefix = "RedmibookManager-" + std::to_string(version[0]) + "." + std::to_string(version[1]) +
                        "." + std::to_string(version[2]);
    if (target == "windows-x86_64") {
        return prefix + "-Windows-x86_64.exe";
    }
    if (target == "linux-x86_64") {
        return prefix + "-Linux-x86_64.deb";
    }
    return {};
}
} // namespace manager::application
