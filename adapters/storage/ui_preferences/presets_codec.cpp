#include "adapters/storage/ui_preferences/presets_codec.hpp"
#include "adapters/storage/ui_preferences/store.hpp"
#include <QJsonDocument>
#include <QJsonObject>

namespace manager::adapters {
namespace {
QJsonObject encode_button(const application::presets::Button &button) {
    QJsonArray actions;
    for (const auto &action : button.actions)
        actions.append(QJsonObject{{"kind", static_cast<int>(action.kind)},
                                   {"value", QString::fromStdString(action.value)}});
    QJsonObject fields{{"id", QString::fromStdString(button.id)},
                       {"title", QString::fromStdString(button.title)},
                       {"icon", QString::fromStdString(button.icon)},
                       {"actions", actions}};
    if (button.default_title != application::presets::DefaultTitle::None)
        fields.insert("defaultTitle", static_cast<int>(button.default_title));
    return fields;
}
} // namespace
QJsonArray encode_presets(const std::vector<application::presets::Preset> &presets) {
    QJsonArray result;
    for (const auto &preset : presets) {
        QJsonArray buttons;
        for (const auto &button : preset.buttons)
            buttons.append(encode_button(button));
        QJsonObject fields{{"id", QString::fromStdString(preset.id)},
                           {"title", QString::fromStdString(preset.title)},
                           {"visible", preset.visible},
                           {"buttons", buttons}};
        if (preset.default_title != application::presets::DefaultTitle::None)
            fields.insert("defaultTitle", static_cast<int>(preset.default_title));
        result.append(fields);
    }
    return result;
}
namespace {
using namespace application::presets;
std::optional<DefaultTitle> decode_title(const QJsonObject &object) {
    if (!object.contains("defaultTitle"))
        return DefaultTitle::None;
    const auto value = object.value("defaultTitle");
    const int title = value.toInt(-1);
    if (!value.isDouble() || value.toDouble() != title || title < 0 || title > 3)
        return std::nullopt;
    return static_cast<DefaultTitle>(title);
}
std::optional<Button> decode_button(const QJsonObject &fields) {
    const auto title = decode_title(fields);
    if (!title || !fields.value("actions").isArray() || fields.value("actions").toArray().size() > 8)
        return std::nullopt;
    Button button{fields.value("id").toString().toStdString(),
                  fields.value("title").toString().toStdString(),
                  fields.value("icon").toString().toStdString(),
                  {},
                  *title};
    for (const auto action : fields.value("actions").toArray()) {
        const auto parts = action.toObject();
        const int kind = parts.value("kind").toInt(-1);
        if (!parts.value("kind").isDouble() || parts.value("kind").toDouble() != kind || kind < 0 || kind > 4)
            return std::nullopt;
        button.actions.push_back({static_cast<Kind>(kind), parts.value("value").toString().toStdString()});
    }
    return button;
}
std::optional<Preset> decode_preset(const QJsonObject &object) {
    const auto title = decode_title(object);
    if (!title || !object.value("visible").isBool() || !object.value("buttons").isArray() ||
        object.value("buttons").toArray().size() > 8)
        return std::nullopt;
    Preset preset{object.value("id").toString().toStdString(),
                  object.value("title").toString().toStdString(),
                  object.value("visible").toBool(),
                  {},
                  *title};
    for (const auto entry : object.value("buttons").toArray()) {
        const auto button = decode_button(entry.toObject());
        if (!button)
            return std::nullopt;
        preset.buttons.push_back(*button);
    }
    return preset;
}
} // namespace

std::optional<std::vector<application::presets::Preset>> decode_presets(const QJsonValue &value) {
    using application::presets::Preset;
    if (value.isUndefined())
        return std::vector<Preset>{};
    if (!value.isArray() || value.toArray().size() > 16)
        return std::nullopt;
    std::vector<Preset> result;
    for (const auto item : value.toArray()) {
        const auto preset = decode_preset(item.toObject());
        if (!preset)
            return std::nullopt;
        result.push_back(*preset);
    }
    return application::presets::valid(result) ? std::optional(result) : std::nullopt;
}
bool UiPreferenceStore::save_tray_presets(const std::vector<application::presets::Preset> &presets) {
    if (!application::presets::valid(presets))
        return false;
    const auto json = QJsonDocument(encode_presets(presets)).toJson(QJsonDocument::Compact);
    return json.size() <= 48000 && update("trayPresets", json.toStdString());
}
} // namespace manager::adapters
