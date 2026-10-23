#pragma once

#include "adapters/platform/environment.hpp"
#include <algorithm>
#include <stdexcept>
#include <string>

namespace manager::adapters {
// An explicit development scope must never silently fall back to installed endpoints.
[[nodiscard]] inline std::string development_scope() {
    const auto value = environment_value("MANAGER_DEV_SCOPE");
    if (!value) {
        return {};
    }
    const auto &scope = *value;
    const bool valid = !scope.empty() && scope.size() <= 48 && std::ranges::all_of(scope, [](char character) {
        return (character >= 'a' && character <= 'z') || (character >= '0' && character <= '9') ||
               character == '-';
    });
    if (!valid) {
        throw std::invalid_argument("Invalid MANAGER_DEV_SCOPE");
    }
    return scope;
}
} // namespace manager::adapters
