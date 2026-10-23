#include "presentation/presets/presets_model.hpp"
#include "presentation/common/icons.hpp"
#include "presentation/common/preset_titles.hpp"
#include <QUuid>
#include <QtConcurrentRun>
#include <algorithm>
#include <utility>

namespace manager::presentation {
namespace {
QString fresh_id() {
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}
QVariantMap encode_action(const application::presets::Action &action) {
    return {{"kind", static_cast<int>(action.kind)}, {"value", QString::fromStdString(action.value)}};
}
std::vector<application::presets::Action> decode_actions(const QVariantList &actions) {
    std::vector<application::presets::Action> result;
    for (const auto &value : actions) {
        const auto map = value.toMap();
        result.push_back({static_cast<application::presets::Kind>(map.value("kind", -1).toInt()),
                          map.value("value").toString().toStdString()});
    }
    return result;
}
} // namespace
PresetsModel::PresetsModel(Preferences &preferences, std::function<application::presets::Catalog()> catalog)
    : preferences_(preferences), catalog_(std::move(catalog)) {
    initialize_icons();
    connect(&preferences_, &Preferences::changed, this, &PresetsModel::changed);
    connect(&preferences_, &Preferences::languageChanged, this, [this] {
        emit changed();
        emit choicesChanged();
    });
    deadline_.setSingleShot(true);
    connect(&deadline_, &QTimer::timeout, this, &PresetsModel::expireChoices);
    connect(&watcher_, &QFutureWatcher<application::presets::Catalog>::finished, this,
            &PresetsModel::completeChoices);
}
void PresetsModel::expireChoices() {
    loading_ = false;
    choices_.clear();
    emit choicesChanged();
    emit errorOccurred(tr("Available actions could not be loaded in time."));
}
void PresetsModel::completeChoices() {
    deadline_.stop();
    loading_ = false;
    try {
        if (elapsed_.elapsed() < 10000)
            choices_ = watcher_.result();
    } catch (...) {
        choices_.clear();
        emit errorOccurred(tr("Could not load available actions."));
    }
    emit choicesChanged();
}
QVariantList PresetsModel::items() const {
    QVariantList result;
    for (const auto &preset : preferences_.tray_presets()) {
        QVariantList buttons;
        for (const auto &button : preset.buttons) {
            QVariantList actions;
            for (const auto &action : button.actions)
                actions.append(encode_action(action));
            buttons.append(QVariantMap{{"id", QString::fromStdString(button.id)},
                                       {"title", preset_title(button.title, button.default_title)},
                                       {"icon", QString::fromStdString(button.icon)},
                                       {"actions", actions}});
        }
        result.append(QVariantMap{{"id", QString::fromStdString(preset.id)},
                                  {"title", preset_title(preset.title, preset.default_title)},
                                  {"visible", preset.visible},
                                  {"buttons", buttons}});
    }
    return result;
}
QString PresetsModel::choice_label(const application::presets::Choice &choice) const {
    QString label;
    using application::presets::Kind;
    if (choice.action.kind == Kind::Performance) {
        const QStringList profiles{tr("Balanced"), tr("Quiet"), tr("Performance")};
        const int index = QString::fromStdString(choice.action.value).toInt() - 1;
        label = profiles.value(index);
    } else if (choice.action.kind == Kind::Refresh)
        label = tr("%1 Hz").arg(QString::fromStdString(choice.action.value));
    else if (choice.action.kind == Kind::Brightness)
        label = tr("%1%").arg(QString::fromStdString(choice.action.value));
    else if (choice.action.kind == Kind::EnergySaver)
        label = choice.action.value == "1" ? tr("On") : tr("Off");
    else
        label = QString::fromStdString(choice.label);
    return label;
}
QVariantList PresetsModel::choices() const {
    QVariantList result;
    for (const auto &choice : choices_) {
        auto item = encode_action(choice.action);
        const auto label = choice_label(choice);
        item.insert("label", label);
        result.append(item);
    }
    return result;
}
void PresetsModel::refreshChoices() {
    if (watcher_.isRunning())
        return;
    loading_ = true;
    emit choicesChanged();
    elapsed_.start();
    deadline_.start(10000);
    watcher_.setFuture(QtConcurrent::run(catalog_));
}
bool PresetsModel::save(const std::vector<application::presets::Preset> &items) {
    if (preferences_.set_tray_presets(items))
        return true;
    emit errorOccurred(tr("Could not save presets. Check names and action limits."));
    return false;
}
void PresetsModel::edit(const QString &id,
                        const std::function<void(application::presets::Preset &)> &change) {
    auto items = preferences_.tray_presets();
    for (auto &preset : items)
        if (preset.id == id.toStdString()) {
            change(preset);
            save(items);
            return;
        }
}
void PresetsModel::addPreset() {
    auto items = preferences_.tray_presets();
    if (items.size() >= 16)
        return;
    items.push_back({fresh_id().toStdString(),
                     tr("Preset %1").arg(items.size() + 1).toStdString(),
                     true,
                     {{fresh_id().toStdString(), tr("First").toStdString(), "quiet", {}},
                      {fresh_id().toStdString(), tr("Second").toStdString(), "turbo", {}}}});
    save(items);
}
void PresetsModel::removePreset(const QString &id) {
    auto items = preferences_.tray_presets();
    std::erase_if(items, [&id](const auto &preset) { return preset.id == id.toStdString(); });
    save(items);
}
void PresetsModel::renamePreset(const QString &id, const QString &title) {
    edit(id, [&title](auto &preset) {
        preset.title = title.trimmed().toStdString();
        preset.default_title = application::presets::DefaultTitle::None;
    });
}
void PresetsModel::showPreset(const QString &id, bool visible) {
    edit(id, [visible](auto &preset) { preset.visible = visible; });
}
void PresetsModel::movePreset(const QString &id, int direction) {
    auto items = preferences_.tray_presets();
    for (std::size_t index = 0; index < items.size(); ++index)
        if (items[index].id == id.toStdString()) {
            const auto next = static_cast<int>(index) + direction;
            if ((direction == -1 || direction == 1) && next >= 0 && next < static_cast<int>(items.size())) {
                std::swap(items[index], items[static_cast<std::size_t>(next)]);
                save(items);
            }
            return;
        }
}
void PresetsModel::addButton(const QString &id) {
    edit(id, [this](auto &preset) {
        if (preset.buttons.size() < 8)
            preset.buttons.push_back({fresh_id().toStdString(),
                                      tr("Button %1").arg(preset.buttons.size() + 1).toStdString(),
                                      "balanced",
                                      {}});
    });
}
void PresetsModel::removeButton(const QString &id, const QString &button) {
    edit(id, [&button](auto &preset) {
        if (preset.buttons.size() > 2)
            std::erase_if(preset.buttons,
                          [&button](const auto &item) { return item.id == button.toStdString(); });
    });
}
void PresetsModel::moveButton(const QString &id, const QString &button, int direction) {
    edit(id, [&button, direction](auto &preset) {
        for (std::size_t index = 0; index < preset.buttons.size(); ++index)
            if (preset.buttons[index].id == button.toStdString()) {
                const auto next = static_cast<int>(index) + direction;
                if ((direction == -1 || direction == 1) && next >= 0 &&
                    next < static_cast<int>(preset.buttons.size()))
                    std::swap(preset.buttons[index], preset.buttons[static_cast<std::size_t>(next)]);
                return;
            }
    });
}
bool PresetsModel::saveButton(const QString &id, const QVariantMap &draft) {
    auto items = preferences_.tray_presets();
    for (auto &preset : items)
        if (preset.id == id.toStdString())
            for (auto &item : preset.buttons)
                if (item.id == draft.value("id").toString().toStdString()) {
                    const auto title = draft.value("title").toString().trimmed();
                    if (title != preset_title(item.title, item.default_title)) {
                        item.title = title.toStdString();
                        item.default_title = application::presets::DefaultTitle::None;
                    }
                    item.icon = draft.value("icon").toString().toStdString();
                    item.actions = decode_actions(draft.value("actions").toList());
                    return save(items);
                }
    return false;
}
} // namespace manager::presentation
