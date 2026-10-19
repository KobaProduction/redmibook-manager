#pragma once
#include "application/updates/release_policy.hpp"
#include <compare>
namespace manager::application {
struct ReleaseIdentity {
    ReleaseVersion version{};
    bool stable{true};
    unsigned sequence{0};
    auto operator<=>(const ReleaseIdentity &) const = default;
};
std::optional<ReleaseIdentity> release_identity(std::string_view text);
ReleaseIdentity installed_release();
} // namespace manager::application
