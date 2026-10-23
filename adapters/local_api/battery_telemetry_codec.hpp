#pragma once
#include "application/battery/telemetry.hpp"
#include <QJsonObject>
namespace manager::adapters {
QJsonObject encode_battery_telemetry(const application::battery::Telemetry &value);
application::battery::Telemetry decode_battery_telemetry(const QJsonObject &object);
} // namespace manager::adapters
