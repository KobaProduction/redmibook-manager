#pragma once
#include "application/ui_preferences/preference_port.hpp"
#include "presentation/common/appearance/theme.hpp"
#include <QObject>
#include <QTranslator>
#include <memory>

class QQmlEngine;

namespace manager::presentation {
class Preferences final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool available READ available NOTIFY connectionChanged)
    Q_PROPERTY(bool connecting READ connecting NOTIFY connectionChanged)
    Q_PROPERTY(QString connectionText READ connectionText NOTIFY connectionChanged)
  public:
    explicit Preferences(application::PreferencePort &store, QObject *parent = nullptr);
    ~Preferences() override;
    void attach_engine(QQmlEngine &engine);
    bool available() const {
        return store_.connection() == application::PreferenceConnection::Ready;
    }
    bool connecting() const {
        return store_.connection() == application::PreferenceConnection::Connecting;
    }
    QString connectionText() const;
    Q_INVOKABLE void reconnect() {
        emit reconnectRequested();
        store_.reconnect();
    }
    [[nodiscard]] QString language() const;
    [[nodiscard]] QString theme() const;
    bool set_language(const QString &value);
    bool set_theme(const QString &value);
    bool reset_defaults() {
        if (!store_.reset_defaults())
            return false;
        refresh();
        return true;
    }
    [[nodiscard]] QString osd_position() const;
    bool set_osd_position(const QString &value);
    [[nodiscard]] bool tray_autostart() const;
    bool set_tray_autostart(bool enabled);
    [[nodiscard]] QString osd_screen() const;
    bool set_osd_screen(const QString &value);
    [[nodiscard]] QString design() const;
    bool set_design(const QString &value);
    [[nodiscard]] bool auto_updates() const;
    bool set_auto_updates(bool enabled);
    bool rounded_corners() const;
    bool set_rounded_corners(bool enabled);
    bool osd_sound() const {
        return value_.osd_sound;
    }
    bool osd_enabled() const {
        return value_.osd_enabled;
    }
    bool set_osd_enabled(bool enabled) {
        if (!store_.update("osdEnabled", enabled ? "true" : "false"))
            return false;
        refresh();
        return true;
    }
    bool set_osd_sound(bool enabled) {
        if (!store_.update("osdSound", enabled ? "true" : "false"))
            return false;
        refresh();
        return true;
    }
    int battery_interval_ms() const {
        return value_.battery_interval_ms;
    }
    bool set_battery_interval_ms(int interval);
    bool tray_section_enabled(std::string_view id) const;
    const std::vector<std::string> &tray_sections() const {
        return value_.tray_sections;
    }
    bool set_tray_section_enabled(const QString &id, bool enabled);
    const std::vector<application::presets::Preset> &tray_presets() const {
        return value_.tray_presets;
    }
    bool set_tray_presets(const std::vector<application::presets::Preset> &presets) {
        if (!store_.save_tray_presets(presets))
            return false;
        refresh();
        return true;
    }
    [[nodiscard]] std::vector<unsigned int> refresh_cycle_rates() const;
    bool set_refresh_cycle_rates(const std::vector<unsigned int> &rates);
    [[nodiscard]] int vendor_key_action() const;
    bool set_vendor_key_action(int action);
    [[nodiscard]] int fn_s_action() const;
    bool set_fn_s_action(int action);
    QPalette palette() const;
    void refresh();
  signals:
    void changed();
    void connectionChanged();
    void reconnectRequested();
    void languageChanged();
    void paletteChanged();
    void errorOccurred(const QString &message);

  private:
    application::PreferencePort &store_;
    application::UiPreferences value_;
    std::unique_ptr<QTranslator> translator_;
    bool initialized_{false};
    ThemeController appearance_;
    QString effective_language_;
    bool apply_language(const QString &requested);
    void apply_theme();
};
} // namespace manager::presentation
