#pragma once
#include "application/presets/model.hpp"
#include <QCoreApplication>
#include <QString>

namespace manager::presentation {
inline QString preset_title(const std::string &custom, application::presets::DefaultTitle standard) {
    using application::presets::DefaultTitle;
    switch (standard) {
    case DefaultTitle::Power:
        return QCoreApplication::translate("PresetDefaults", "Power");
    case DefaultTitle::Economy:
        return QCoreApplication::translate("PresetDefaults", "Economy");
    case DefaultTitle::Performance:
        return QCoreApplication::translate("PresetDefaults", "Performance");
    case DefaultTitle::None:
        return QString::fromStdString(custom);
    }
    return QString::fromStdString(custom);
}
} // namespace manager::presentation
