#pragma once
#include "application/presets/icons.hpp"
#include <QCoreApplication>
#include <QVariantList>
#include <QVariantMap>
#include <array>

namespace manager::presentation {
inline QVariantList preset_icon_choices() {
    constexpr std::array labels{
        QT_TRANSLATE_NOOP("PresetIcons", "Balanced"),     QT_TRANSLATE_NOOP("PresetIcons", "Quiet"),
        QT_TRANSLATE_NOOP("PresetIcons", "Performance"),  QT_TRANSLATE_NOOP("PresetIcons", "Display"),
        QT_TRANSLATE_NOOP("PresetIcons", "Charge limit"), QT_TRANSLATE_NOOP("PresetIcons", "Manager"),
        QT_TRANSLATE_NOOP("PresetIcons", "Microphone"),   QT_TRANSLATE_NOOP("PresetIcons", "Mute microphone"),
        QT_TRANSLATE_NOOP("PresetIcons", "Battery"),      QT_TRANSLATE_NOOP("PresetIcons", "Charger"),
        QT_TRANSLATE_NOOP("PresetIcons", "Power"),        QT_TRANSLATE_NOOP("PresetIcons", "Night"),
        QT_TRANSLATE_NOOP("PresetIcons", "Brightness"),   QT_TRANSLATE_NOOP("PresetIcons", "Cooling"),
        QT_TRANSLATE_NOOP("PresetIcons", "Sound"),        QT_TRANSLATE_NOOP("PresetIcons", "Headphones"),
        QT_TRANSLATE_NOOP("PresetIcons", "Keyboard"),     QT_TRANSLATE_NOOP("PresetIcons", "Wi-Fi"),
        QT_TRANSLATE_NOOP("PresetIcons", "Bluetooth"),    QT_TRANSLATE_NOOP("PresetIcons", "Lock"),
        QT_TRANSLATE_NOOP("PresetIcons", "Gaming"),       QT_TRANSLATE_NOOP("PresetIcons", "Video"),
        QT_TRANSLATE_NOOP("PresetIcons", "Development"),  QT_TRANSLATE_NOOP("PresetIcons", "Work"),
        QT_TRANSLATE_NOOP("PresetIcons", "Break"),
    };
    static_assert(labels.size() == application::presets::icon_ids.size());
    QVariantList result;
    for (std::size_t i = 0; i < labels.size(); ++i)
        result.append(QVariantMap{{"id", QString::fromUtf8(application::presets::icon_ids[i].data())},
                                  {"label", QCoreApplication::translate("PresetIcons", labels[i])}});
    return result;
}
} // namespace manager::presentation
