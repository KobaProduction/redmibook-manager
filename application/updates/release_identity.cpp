#include "application/updates/release_identity.hpp"
#include "application/runtime/build_identity.hpp"
#include <charconv>
#include <limits>
namespace manager::application {
std::optional<ReleaseIdentity> release_identity(std::string_view text) {
    const auto separator = text.find("-alpha.");
    const auto version = release_version(text.substr(0, separator));
    if (!version) {
        return std::nullopt;
    }
    ReleaseIdentity result{.version = *version};
    if (separator == std::string_view::npos) {
        return result;
    }
    const auto suffix = text.substr(separator + 7);
    unsigned sequence = 0;
    const auto parsed = std::from_chars(suffix.data(), suffix.data() + suffix.size(), sequence);
    if (suffix.empty() || parsed.ec != std::errc{} || parsed.ptr != suffix.data() + suffix.size()) {
        return std::nullopt;
    }
    result.stable = false;
    result.sequence = sequence;
    return result;
}
ReleaseIdentity installed_release() {
    const bool stable = std::string_view(manager::application::build_identity::channel) == "stable";
    return {.version =
                release_version(manager::application::build_identity::version).value_or(ReleaseVersion{}),
            .stable = stable,
            .sequence = stable ? 0U : manager::application::build_identity::sequence};
}
} // namespace manager::application
