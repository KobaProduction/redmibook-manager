#include "adapters/local_api/battery_telemetry_codec.hpp"
#include <cmath>
namespace manager::adapters {
namespace {
template <class T> QJsonValue optional(const std::optional<T> &value) {
    return value ? QJsonValue(*value) : QJsonValue();
}
std::optional<double> number(const QJsonObject &object, const char *key, double minimum, double maximum) {
    const auto item = object.value(QLatin1String(key));
    const double value = item.toDouble();
    if (!item.isDouble() || !std::isfinite(value) || value < minimum || value > maximum)
        return {};
    return value;
}
std::optional<bool> boolean(const QJsonObject &object, const char *key) {
    const auto value = object.value(QLatin1String(key));
    return value.isBool() ? std::optional<bool>(value.toBool()) : std::nullopt;
}
} // namespace
QJsonObject encode_battery_telemetry(const application::battery::Telemetry &value) {
    return {{"status", static_cast<int>(value.status)}, {"online", optional(value.online)},
            {"charging", optional(value.charging)},     {"discharging", optional(value.discharging)},
            {"percent", optional(value.percent)},       {"remainingWh", optional(value.remaining_wh)},
            {"fullWh", optional(value.full_wh)},        {"designWh", optional(value.design_wh)},
            {"voltage", optional(value.voltage)},       {"rateW", optional(value.rate_w)}};
}
application::battery::Telemetry decode_battery_telemetry(const QJsonObject &object) {
    application::battery::Telemetry result;
    const int status = object.value("status").toInt(-1);
    if (status < 0 || status > 3)
        return result;
    result.status = static_cast<application::battery::TelemetryStatus>(status);
    result.online = boolean(object, "online");
    result.charging = boolean(object, "charging");
    result.discharging = boolean(object, "discharging");
    if (const auto percent = number(object, "percent", 0, 100))
        result.percent = static_cast<int>(*percent);
    result.remaining_wh = number(object, "remainingWh", 0, 10000);
    result.full_wh = number(object, "fullWh", 0, 10000);
    result.design_wh = number(object, "designWh", 0, 10000);
    result.voltage = number(object, "voltage", 0, 1000);
    result.rate_w = number(object, "rateW", -10000, 10000);
    return result;
}
} // namespace manager::adapters
