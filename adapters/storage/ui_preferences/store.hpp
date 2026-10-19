#pragma once
#include "application/ui_preferences/preference_port.hpp"
#include <QFileSystemWatcher>
#include <QObject>
#include <QString>
#include <optional>

namespace manager::adapters {
class UiPreferenceStore final : public QObject, public application::PreferencePort {
  public:
    explicit UiPreferenceStore(QString directory);
    [[nodiscard]] std::optional<application::UiPreferences> read() const override;
    bool update(std::string_view key, std::string_view value) override;
    void observe(std::function<void()> changed) override;
    [[nodiscard]] const QString &directory() const {
        return directory_;
    }

  private:
    QString directory_;
    QFileSystemWatcher watcher_;
    std::function<void()> changed_;
    [[nodiscard]] QString path() const;
    void notify_change();
};
} // namespace manager::adapters
