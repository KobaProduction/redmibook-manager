#pragma once
#include "application/ui_preferences/preference_port.hpp"
#include <QFutureWatcher>
#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QTimer>
#include <optional>

namespace manager::adapters {
class UiPreferenceStore final : public QObject, public application::PreferencePort {
  public:
    explicit UiPreferenceStore(QString directory, bool authority = false);
    ~UiPreferenceStore() override;
    QByteArray snapshot() const;
    static std::optional<application::UiPreferences> decode_snapshot(const QByteArray &bytes);
    void observe_error(const std::function<void()> &error) override {
        error_ = error;
    }
    [[nodiscard]] std::optional<application::UiPreferences> read() const override;
    bool update(std::string_view key, std::string_view value) override;
    bool reset_defaults() override;
    bool configure_starter_defaults(application::presets::Catalog catalog);
    bool save_tray_presets(const std::vector<application::presets::Preset> &presets) override;
    void observe(std::function<void()> changed) override;
    [[nodiscard]] const QString &directory() const {
        return directory_;
    }

  private:
    QString directory_;
    bool authority_{false};
    std::optional<QJsonObject> memory_;
    std::optional<application::presets::Catalog> starter_catalog_;
    QTimer save_timer_;
    QFutureWatcher<bool> saving_;
    bool dirty_{false};
    bool save_error_reported_{false};
    std::function<void()> error_;
    std::function<void()> changed_;
    [[nodiscard]] QString path() const;
    void notify_change();
    bool commit_object(const QJsonObject &object);
    void flush();
    void save_finished();
};
} // namespace manager::adapters
