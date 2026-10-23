#include "presentation/device_controls_model.hpp"
#include <algorithm>
#include <cmath>
namespace manager::presentation {
namespace {
void field(QVariantMap &result, const char *key, const std::optional<double> &value) {
    if (value)
        result.insert(QLatin1String(key), *value);
}
} // namespace
QString DeviceControlsModel::batteryState() const {
    const auto &value = battery_telemetry_;
    QString state = tr("Battery state unknown");
    if (value.charging.value_or(false))
        state = tr("Charging");
    else if (value.discharging.value_or(false))
        state = tr("Discharging");
    else if (value.online.value_or(false))
        state = tr("Plugged in, not charging");
    return state;
}
QVariantMap DeviceControlsModel::batteryTelemetry() const {
    const auto &value = battery_telemetry_;
    if (value.status != application::battery::TelemetryStatus::Available)
        return {};
    QVariantMap result{{QStringLiteral("state"), batteryState()}};
    const bool discharge =
        value.rate_w ? (*value.rate_w < 0 || (*value.rate_w == 0 && value.discharging.value_or(false)))
                     : value.discharging.value_or(false);
    result.insert(QStringLiteral("rateLabel"), discharge ? tr("Discharge") : tr("Charge"));
    field(result, "remainingWh", value.remaining_wh);
    field(result, "fullWh", value.full_wh);
    field(result, "designWh", value.design_wh);
    field(result, "voltage", value.voltage);
    field(result, "rateW", value.rate_w);
    if (value.full_wh && value.design_wh && *value.design_wh > 0)
        result.insert(QStringLiteral("wear"),
                      std::clamp(100.0 * (1 - *value.full_wh / *value.design_wh), 0.0, 100.0));
    return result;
}
} // namespace manager::presentation
