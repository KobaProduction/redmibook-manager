#pragma once
#include "application/ui_preferences/preference_port.hpp"
#include <QtGlobal>
namespace manager::desktop {
// Isolated loading-screen fixture: no IPC, persistence, or native writes.
class LoadingPreview final : public application::PreferencePort {
  public:
    std::optional<application::UiPreferences> read() const override {
        application::UiPreferences value;
        value.theme =
            qEnvironmentVariable("MANAGER_PREVIEW_THEME") == QStringLiteral("light") ? "light" : "dark";
        return value;
    }
    application::PreferenceConnection connection() const override {
        return application::PreferenceConnection::Connecting;
    }
    bool update(std::string_view, std::string_view) override {
        return false;
    }
    void observe(std::function<void()>) override {}
};
} // namespace manager::desktop
