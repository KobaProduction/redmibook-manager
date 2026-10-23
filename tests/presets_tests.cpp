#include "adapters/storage/ui_preferences/store.hpp"
#include "application/presets/model.hpp"
#include "application/presets/starter.hpp"
#include "presentation/common/preset_titles.hpp"
#include "presentation/presets/presets_model.hpp"
#include <QApplication>
#include <QTemporaryDir>

namespace {
using namespace manager::application::presets;
class Executor final : public ExecutorPort {
  public:
    Catalog available{{{Kind::Refresh, "60"}, {}}, {{Kind::Performance, "2"}, {}}};
    unsigned int calls{};
    unsigned int fail_at{99};
    Catalog choices() override {
        return available;
    }
    bool apply(const Action &) override {
        return ++calls != fail_at;
    }
};
bool execution() {
    Executor executor;
    Button button{"button", "Energy saver", "quiet", {{Kind::Refresh, "60"}, {Kind::Performance, "2"}}};
    const auto complete = execute(button, executor);
    if (!complete.confirmed || complete.completed != 2 || executor.calls != 2)
        return false;
    executor.calls = 0;
    executor.fail_at = 2;
    const auto partial = execute(button, executor);
    if (partial.confirmed || partial.completed != 1 || executor.calls != 2)
        return false;
    executor.calls = 0;
    button.actions.push_back({Kind::Refresh, "165"});
    const auto missing = execute(button, executor);
    if (!missing.unavailable || missing.completed != 0 || executor.calls != 0)
        return false;
    button.actions.clear();
    return execute(button, executor).confirmed && executor.calls == 0;
}
bool deadline_stops_sequence() {
    Executor executor;
    Button button{"deadline", "Deadline", "quiet", {{Kind::Refresh, "60"}, {Kind::Performance, "2"}}};
    unsigned int starts{};
    const auto result = execute(button, executor, [&starts] { return ++starts == 1; });
    return !result.confirmed && result.completed == 1 && executor.calls == 1;
}
bool starter_contract() {
    const Catalog available{
        {{Kind::Refresh, "60"}, {}},    {{Kind::Refresh, "120"}, {}},   {{Kind::Refresh, "165"}, {}},
        {{Kind::Performance, "2"}, {}}, {{Kind::Performance, "3"}, {}}, {{Kind::EnergySaver, "1"}, {}},
        {{Kind::EnergySaver, "0"}, {}}, {{Kind::Brightness, "50"}, {}}, {{Kind::Brightness, "100"}, {}}};
    const auto example = starter(available);
    const auto limited = starter(available, {60, 120});
    const auto unsupported = starter({});
    return valid({example}) && !example.visible && example.buttons.front().title == "Economy" &&
           example.buttons.back().title == "Performance" && example.buttons.front().actions.size() == 4 &&
           example.buttons.back().actions.size() == 4 &&
           example.buttons.back().actions.front() == Action{Kind::Refresh, "165"} &&
           limited.buttons.back().actions.front() == Action{Kind::Refresh, "120"} &&
           unsupported.buttons.front().actions.empty() && unsupported.buttons.back().actions.empty();
}
bool edit_buttons(manager::presentation::PresetsModel &model, const QString &id) {
    const auto initial = model.items().first().toMap().value("buttons").toList();
    if (initial.size() != 2 || !initial.first().toMap().value("actions").toList().empty())
        return false;
    model.removeButton(id, initial.first().toMap().value("id").toString());
    if (model.items().first().toMap().value("buttons").toList().size() != 2)
        return false;
    model.addButton(id);
    const auto buttons = model.items().first().toMap().value("buttons").toList();
    const auto button = buttons.last().toMap().value("id").toString();
    const QVariantList actions{QVariantMap{{"kind", 1}, {"value", "60"}},
                               QVariantMap{{"kind", 0}, {"value", "2"}}};
    if (!model.saveButton(
            id,
            {{"id", button}, {"title", QString(120, QChar('x'))}, {"icon", "display"}, {"actions", actions}}))
        return false;
    model.moveButton(id, button, -1);
    return !model.saveButton(
        id, {{"id", button}, {"title", "bad"}, {"icon", "not-an-icon"}, {"actions", actions}});
}
bool persisted_and_ordered(manager::presentation::PresetsModel &model,
                           manager::adapters::UiPreferenceStore &store, const QString &id) {
    model.showPreset(id, false);
    const auto loaded = store.read();
    if (!loaded || loaded->tray_presets.size() != 1 || loaded->tray_presets.front().visible ||
        loaded->tray_presets.front().buttons[1].actions.size() != 2)
        return false;
    model.addPreset();
    model.movePreset(id, 1);
    if (model.items().last().toMap().value("id").toString() != id)
        return false;
    model.removePreset(id);
    model.removePreset(model.items().first().toMap().value("id").toString());
    const auto empty = store.read();
    return empty && empty->tray_presets.empty();
}
bool configuration() {
    QTemporaryDir directory;
    manager::adapters::UiPreferenceStore store(directory.path());
    manager::presentation::Preferences preferences(store);
    manager::presentation::PresetsModel model(preferences, [] { return Catalog{}; });
    if (!model.items().empty())
        return false;
    model.addPreset();
    if (model.items().empty())
        return false;
    const auto id = model.items().first().toMap().value("id").toString();
    return edit_buttons(model, id) && persisted_and_ordered(model, store, id);
}
bool customized_defaults(manager::presentation::PresetsModel &model,
                         manager::presentation::Preferences &preferences,
                         manager::adapters::UiPreferenceStore &store) {
    const auto preset = model.items().first().toMap();
    auto button = preset.value("buttons").toList().first().toMap();
    model.renamePreset(preset.value("id").toString(), "My power");
    button.insert("title", "My button");
    if (!model.saveButton(preset.value("id").toString(), button) || !preferences.set_language("ru"))
        return false;
    const auto renamed = model.items().first().toMap();
    const auto observed = store.read();
    return observed && !observed->tray_presets.empty() && renamed.value("title") == "My power" &&
           renamed.value("buttons").toList().first().toMap().value("title") == "My button" &&
           renamed.value("buttons").toList().last().toMap().value("title") ==
               QString::fromUtf8("Производительность") &&
           !observed->tray_presets.front().visible;
}
bool translated_defaults() {
    QTemporaryDir directory;
    manager::adapters::UiPreferenceStore store(directory.path());
    if (!store.configure_starter_defaults({}))
        return false;
    manager::presentation::Preferences preferences(store);
    manager::presentation::PresetsModel model(preferences, [] { return Catalog{}; });
    if (!preferences.set_language("ru") ||
        model.items().first().toMap().value("title") != QString::fromUtf8("Питание"))
        return false;
    const auto preset = model.items().first().toMap();
    const auto button = preset.value("buttons").toList().first().toMap();
    if (button.value("title") != QString::fromUtf8("Экономия") ||
        !model.saveButton(preset.value("id").toString(), button) || !preferences.set_language("en"))
        return false;
    return model.items().first().toMap().value("buttons").toList().first().toMap().value("title") ==
               "Economy" &&
           customized_defaults(model, preferences, store);
}
bool migrated_defaults() {
    Preset legacy{"starter-energy",
                  "Energy",
                  true,
                  {{"starter-min", "Min", "quiet", {}}, {"starter-max", "My custom", "turbo", {}}}};
    migrate_starter(legacy);
    return !legacy.visible && legacy.default_title == DefaultTitle::Power &&
           legacy.buttons.front().default_title == DefaultTitle::Economy &&
           legacy.buttons.back().default_title == DefaultTitle::None &&
           legacy.buttons.back().title == "My custom";
}
} // namespace
int main(int argc, char **argv) {
    QApplication application(argc, argv);
    return execution() && deadline_stops_sequence() && starter_contract() && configuration() &&
                   translated_defaults() && migrated_defaults()
               ? 0
               : 1;
}
