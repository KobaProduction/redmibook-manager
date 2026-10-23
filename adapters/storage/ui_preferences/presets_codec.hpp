#pragma once
#include "application/presets/model.hpp"
#include <QJsonArray>
#include <optional>
namespace manager::adapters {
QJsonArray encode_presets(const std::vector<application::presets::Preset> &presets);
std::optional<std::vector<application::presets::Preset>> decode_presets(const QJsonValue &value);
} // namespace manager::adapters
