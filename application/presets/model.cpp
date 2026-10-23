#include "application/presets/model.hpp"
#include "application/presets/icons.hpp"
#include <algorithm>
#include <array>
#include <charconv>
#include <set>

namespace manager::application::presets {
namespace {
bool identifier(const std::string &id) {
    return !id.empty() && id.size() <= 64 && std::ranges::all_of(id, [](unsigned char c) {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-';
    });
}
bool action_valid(const Action &action) {
    if (action.kind == Kind::EnergySaver)
        return action.value == "0" || action.value == "1";
    if (action.kind == Kind::Performance)
        return action.value == "1" || action.value == "2" || action.value == "3";
    if (action.kind == Kind::PowerScheme)
        return action.value.size() == 38 && action.value.front() == '{' && action.value.back() == '}';
    if (action.kind != Kind::Refresh && action.kind != Kind::Brightness)
        return false;
    unsigned int hz{};
    const auto result = std::from_chars(action.value.data(), action.value.data() + action.value.size(), hz);
    const bool range = action.kind == Kind::Brightness ? hz <= 100 : hz >= 2 && hz <= 1000;
    return result.ec == std::errc{} && result.ptr == action.value.data() + action.value.size() && range;
}
bool button_valid(const Button &button) {
    if (!identifier(button.id) || button.title.empty() || button.title.size() > 256 ||
        std::ranges::find(icon_ids, button.icon) == icon_ids.end() || button.actions.size() > 8 ||
        button.default_title > DefaultTitle::Performance)
        return false;
    return std::ranges::all_of(button.actions, action_valid);
}
} // namespace
bool valid(const std::vector<Preset> &presets) {
    if (presets.size() > 16)
        return false;
    std::set<std::string> ids;
    for (const auto &preset : presets) {
        if (!identifier(preset.id) || !ids.insert(preset.id).second || preset.title.empty() ||
            preset.title.size() > 256 || preset.buttons.size() < 2 || preset.buttons.size() > 8 ||
            preset.default_title > DefaultTitle::Performance)
            return false;
        std::set<std::string> buttons;
        for (const auto &button : preset.buttons)
            if (!button_valid(button) || !buttons.insert(button.id).second)
                return false;
    }
    return true;
}
namespace {
bool supported(const Button &button, const Catalog &choices) {
    return std::ranges::all_of(button.actions, [&choices](const Action &action) {
        return std::ranges::any_of(choices,
                                   [&action](const Choice &choice) { return choice.action == action; });
    });
}
} // namespace
Result execute(const Button &button, ExecutorPort &executor, const std::function<bool()> &proceed) {
    if (!button_valid(button))
        return {};
    if (!supported(button, executor.choices()))
        return {false, 0, true};
    Result result;
    for (const auto &action : button.actions) {
        if (proceed && !proceed())
            return result;
        if (!executor.apply(action))
            return result;
        ++result.completed;
    }
    result.confirmed = true;
    return result;
}
} // namespace manager::application::presets
