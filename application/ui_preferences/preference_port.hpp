#pragma once
#include "application/ui_preferences/preferences.hpp"
#include <functional>
#include <optional>
#include <string_view>

namespace manager::application {
class PreferencePort {
  public:
    virtual ~PreferencePort() = default;
    [[nodiscard]] virtual std::optional<UiPreferences> read() const = 0;
    virtual bool update(std::string_view key, std::string_view value) = 0;
    // One composition-owned observer, invoked on the caller's event thread.
    // Clear before destroying its receiver. Implementations serialize file writers.
    virtual void observe(std::function<void()> changed) = 0;
};
} // namespace manager::application
