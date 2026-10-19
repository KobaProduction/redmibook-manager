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
  public:
    explicit Preferences(application::PreferencePort &store, QObject *parent = nullptr);
    ~Preferences() override;
    void attach_engine(QQmlEngine &engine);
    [[nodiscard]] QString language() const;
    [[nodiscard]] QString theme() const;
    bool set_language(const QString &value);
    bool set_theme(const QString &value);
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
    QPalette palette() const;
    void refresh();
  signals:
    void changed();
    void languageChanged();
    void paletteChanged();

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
