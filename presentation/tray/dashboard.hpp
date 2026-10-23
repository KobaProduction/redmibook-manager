#pragma once
#include "application/runtime/session_port.hpp"
#include "application/ui_preferences/preferences.hpp"
#include "presentation/common/rounded_frame.hpp"
#include "presentation/common/runtime_monitor.hpp"
#include "presentation/tray/preset_panel.hpp"
#include <QButtonGroup>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QString>
#include <QToolButton>
#include <QVBoxLayout>
#include <array>
#include <optional>
#include <vector>

namespace manager::presentation {
struct TrayDeviceState {
    int profileIndex{-1};
    bool profileAvailable{false};
    int refreshHz{0};
    bool refreshAvailable{false};
    bool microphoneAvailable{false};
    bool microphoneMuted{false};
    bool pending{false};
    QString status;
    std::vector<unsigned int> refreshRates{};
    bool error{false};
    int chargeLimit{-1};
    std::optional<double> batteryRateW{};
    bool batteryDischarging{false};
    std::vector<std::string> sections{application::UiPreferences{}.tray_sections};
    std::vector<application::presets::Preset> presets{};
    bool profileSupported{false};
    bool refreshSupported{false};
    bool operator==(const TrayDeviceState &) const = default;
};

class TrayDashboard final : public RoundedFrame {
    Q_OBJECT
  public:
    explicit TrayDashboard(RuntimeMonitor &runtime);
    void toggle(const QRect &anchor);
    void retranslate();
    void setDeviceState(TrayDeviceState state);
    void setPreferenceConnection(bool available, const QString &text);
  signals:
    void openManager();
    void reconnectRequested();
    void profileRequested(int index);
    void refreshRequested(unsigned int hz);
    void presetRequested(const QString &preset, const QString &button);
    void errorOccurred(const QString &message);
    void visibilityChanged(bool visible);

  protected:
    void changeEvent(QEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;

  private:
    RuntimeMonitor &runtime_;
    bool preferences_available_{true};
    QLabel connection_notice_;
    QPushButton reconnect_;
    QLabel title_;
    QLabel version_;
    QLabel status_;
    QLabel microphone_icon_;
    QWidget battery_box_;
    QLabel battery_icon_;
    QLabel battery_power_;
    QWidget performance_section_;
    QWidget refresh_section_;
    QScrollArea preset_scroll_;
    PresetPanel presets_;
    QLabel refresh_label_;
    std::vector<std::pair<std::string_view, QWidget *>> configurable_sections_;
    std::array<QToolButton, 3> refresh_buttons_;
    static constexpr std::array<unsigned int, 3> refresh_choices{60, 120, 165};
    QLabel mode_label_;
    std::array<QToolButton, 3> modes_;
    QButtonGroup mode_group_;
    QPushButton open_;
    TrayDeviceState device_state_;
    QRect anchor_;
    void build_layout();
    void build_body(QVBoxLayout &layout);
    void update_presets();
    bool battery_only_change(const TrayDeviceState &after) const;
    void fit_to_screen(const QRect &area);
    void place_dashboard();

    QVBoxLayout *build_header();
    void connect_actions();
    void configure_header();
    void build_modes(QVBoxLayout &layout);
    void build_refresh(QVBoxLayout &layout);
    void update_refresh();
    void update_device_controls();
    QString microphone_text() const;
    void update_microphone_icon();
    void update_runtime_icon();
    void update_battery(bool redrawIcon = true);
    void update_sections();
    bool section_enabled(std::string_view id) const;
};
} // namespace manager::presentation
