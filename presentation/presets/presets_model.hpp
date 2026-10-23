#pragma once
#include "presentation/common/preferences.hpp"
#include "presentation/presets/icon_choices.hpp"
#include <QElapsedTimer>
#include <QFutureWatcher>
#include <QTimer>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>
#include <functional>

namespace manager::presentation {
class PresetsModel final : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Created by application composition")
    Q_PROPERTY(QVariantList items READ items NOTIFY changed)
    Q_PROPERTY(QVariantList choices READ choices NOTIFY choicesChanged)
    Q_PROPERTY(QVariantList iconChoices READ iconChoices NOTIFY changed)
    Q_PROPERTY(bool loading READ loading NOTIFY choicesChanged)
  public:
    PresetsModel(Preferences &preferences, std::function<application::presets::Catalog()> catalog);
    QVariantList items() const;
    QVariantList choices() const;
    QVariantList iconChoices() const {
        return preset_icon_choices();
    }
    bool loading() const {
        return loading_;
    }
    Q_INVOKABLE void refreshChoices();
    Q_INVOKABLE void addPreset();
    Q_INVOKABLE void removePreset(const QString &id);
    Q_INVOKABLE void renamePreset(const QString &id, const QString &title);
    Q_INVOKABLE void showPreset(const QString &id, bool visible);
    Q_INVOKABLE void movePreset(const QString &id, int direction);
    Q_INVOKABLE void addButton(const QString &id);
    Q_INVOKABLE void removeButton(const QString &id, const QString &button);
    Q_INVOKABLE void moveButton(const QString &id, const QString &button, int direction);
    Q_INVOKABLE bool saveButton(const QString &id, const QVariantMap &draft);
  signals:
    void changed();
    void choicesChanged();
    void errorOccurred(const QString &message);

  private:
    QString choice_label(const application::presets::Choice &choice) const;
    Preferences &preferences_;
    std::function<application::presets::Catalog()> catalog_;
    QFutureWatcher<application::presets::Catalog> watcher_;
    QTimer deadline_;
    QElapsedTimer elapsed_;
    application::presets::Catalog choices_;
    bool loading_{false};
    void completeChoices();
    void expireChoices();
    bool save(const std::vector<application::presets::Preset> &items);
    void edit(const QString &id, const std::function<void(application::presets::Preset &)> &change);
};
} // namespace manager::presentation
