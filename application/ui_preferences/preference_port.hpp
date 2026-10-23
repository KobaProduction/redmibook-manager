#pragma once
#include "application/ui_preferences/preferences.hpp"
#include <cstdint>
#include <functional>
#include <optional>
#include <string_view>

namespace manager::application {
enum class PreferenceConnection : std::uint8_t { Ready, Connecting, Unavailable, Incompatible };
class PreferencePort {
  public:
    virtual ~PreferencePort() = default;
    virtual PreferenceConnection connection() const {
        return PreferenceConnection::Ready;
    }
    virtual void reconnect() {}
    virtual void observe_connection(const std::function<void()> &) {}
    [[nodiscard]] virtual std::optional<UiPreferences> read() const = 0;
    virtual bool update(std::string_view key, std::string_view value) = 0;
    virtual void observe_error(const std::function<void()> &) {}
    // Reset owned user preferences atomically; machine/hardware policy is separate.
    virtual bool reset_defaults() {
        return false;
    }
    virtual bool save_tray_presets(const std::vector<presets::Preset> &) {
        return false;
    }
    // One composition-owned observer, invoked on the caller's event thread.
    // Clear before destroying its receiver. Client mutations are accepted IPC requests;
    // the daemon publishes authoritative revisions and reports persistence failures.
    virtual void observe(std::function<void()> changed) = 0;
};
} // namespace manager::application
