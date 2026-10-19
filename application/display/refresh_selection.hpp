#pragma once

#include <algorithm>
#include <optional>
#include <span>
#include <vector>

namespace manager::application::display {

[[nodiscard]] inline bool valid_refresh_selection(const std::vector<unsigned int> &rates) {
    if (rates.empty() || rates.size() > 32)
        return false;
    auto unique = rates;
    std::ranges::sort(unique);
    return unique.front() >= 2 && unique.back() <= 1000 &&
           std::adjacent_find(unique.begin(), unique.end()) == unique.end();
}

[[nodiscard]] inline std::vector<unsigned int>
selected_refresh_rates(std::span<const unsigned int> advertised,
                       const std::vector<unsigned int> &configured) {
    if (!valid_refresh_selection(configured))
        return {};
    auto selected = configured;
    std::ranges::sort(selected);
    const bool default_selection = selected == std::vector<unsigned int>{60, 165};
    if (default_selection && std::ranges::find(advertised, 165) == advertised.end() &&
        std::ranges::find(advertised, 120) != advertised.end())
        selected = {60, 120};
    std::erase_if(selected, [advertised](unsigned int hz) {
        return std::ranges::find(advertised, hz) == advertised.end();
    });
    return selected;
}

[[nodiscard]] inline std::optional<unsigned int>
next_refresh_rate(unsigned int current_hz, const std::vector<unsigned int> &selected) {
    if (!valid_refresh_selection(selected))
        return {};
    auto sorted = selected;
    std::ranges::sort(sorted);
    const auto next = std::ranges::upper_bound(sorted, current_hz);
    return next == sorted.end() ? sorted.front() : *next;
}

} // namespace manager::application::display
