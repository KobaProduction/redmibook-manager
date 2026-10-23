#pragma once
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace manager::application::presets {
enum class DefaultTitle : std::uint8_t { None, Power, Economy, Performance };
enum class Kind : std::uint8_t {
    Performance = 0,
    Refresh = 1,
    PowerScheme = 2,
    EnergySaver = 3,
    Brightness = 4
};
struct Action {
    Kind kind{Kind::Performance};
    std::string value;
    bool operator==(const Action &) const = default;
};
struct Button {
    std::string id;
    std::string title;
    std::string icon{"balanced"};
    std::vector<Action> actions{};
    DefaultTitle default_title{DefaultTitle::None};
    bool operator==(const Button &) const = default;
};
struct Preset {
    std::string id;
    std::string title;
    bool visible{true};
    std::vector<Button> buttons{};
    DefaultTitle default_title{DefaultTitle::None};
    bool operator==(const Preset &) const = default;
};
struct Choice {
    Action action;
    std::string label;
};
using Catalog = std::vector<Choice>;
bool valid(const std::vector<Preset> &presets);
class ExecutorPort {
  public:
    virtual ~ExecutorPort() = default;
    virtual Catalog choices() = 0;
    virtual bool apply(const Action &action) = 0;
};
struct Result {
    bool confirmed{false};
    unsigned int completed{0};
    bool unavailable{false};
};
// Preflight the complete sequence before writes; stop at the first unconfirmed action.
// This is ordered execution, not an atomic transaction. No automatic rollback.
Result execute(const Button &button, ExecutorPort &executor, const std::function<bool()> &proceed = {});
} // namespace manager::application::presets
