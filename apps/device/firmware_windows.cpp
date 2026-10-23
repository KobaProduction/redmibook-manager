#include "adapters/device_profile/native_profile.hpp"
#include "adapters/platform/firmware_controls.hpp"
#include "adapters/windows/battery/charge_control.hpp"
#include "adapters/windows/firmware/microphone_state.hpp"
#include "adapters/windows/performance/performance_control.hpp"
#include "apps/device/firmware.hpp"
#include "apps/device/qualification.hpp"
#include <QJsonArray>

namespace manager::device {
namespace {
adapters::windows_firmware::FirmwareIdentity identity() {
    const auto profile = adapters::NativeDeviceProfile{}.read();
    return {profile.manufacturer, profile.board_model, profile.bios_version};
}
QJsonObject charge_json(const application::battery::Observation &value) {
    return {
        {QStringLiteral("confirmed"), value.status == application::battery::Status::Confirmed},
        {QStringLiteral("status"), static_cast<int>(value.status)},
        {QStringLiteral("detail"), QString::fromStdString(value.detail)},
        {QStringLiteral("limit"), value.limit ? QJsonValue(static_cast<int>(*value.limit)) : QJsonValue()}};
}
QJsonObject performance_json(const application::performance::Observation &value) {
    return {{QStringLiteral("confirmed"), value.status == application::performance::Status::Confirmed},
            {QStringLiteral("status"), static_cast<int>(value.status)},
            {QStringLiteral("detail"), QString::fromStdString(value.detail)},
            {QStringLiteral("profile"),
             value.profile ? QJsonValue(static_cast<int>(*value.profile)) : QJsonValue()}};
}
} // namespace
QJsonObject firmware_status() {
    adapters::windows_firmware::NativeWmiTransport transport;
    adapters::windows_battery::ChargeControl battery(transport, identity());
    adapters::windows_performance::PerformanceControl performance(transport, identity());
    const auto capability = battery.capability();
    QJsonArray allowed;
    for (const auto limit : capability.allowed_limits) {
        allowed.append(static_cast<int>(limit));
    }
    return {{QStringLiteral("charge"), charge_json(battery.read())},
            {QStringLiteral("chargeCapability"), static_cast<int>(capability.status)},
            {QStringLiteral("allowedLimits"), allowed},
            {QStringLiteral("performance"), performance_json(performance.read())},
            {QStringLiteral("performanceCapability"), static_cast<int>(performance.capability())}};
}
namespace {
QJsonObject microphone_firmware_status() {
    adapters::windows_firmware::NativeWmiTransport transport;
    const auto state = adapters::windows_firmware::read_microphone_state(transport, identity());
    return {{"known", state.status == application::microphone::FirmwareStateStatus::Ready &&
                          state.desired_muted.has_value()},
            {"source", "firmware-intent"},
            {"status", static_cast<int>(state.status)},
            {"desiredMuted", state.desired_muted ? QJsonValue(*state.desired_muted) : QJsonValue()},
            {"detail", QString::fromStdString(state.detail)}};
}
QJsonObject charge_command(const QString &value) {
    bool valid{};
    const auto percent = value.toInt(&valid);
    if (!valid || (percent != 80 && percent != 100)) {
        return {{QStringLiteral("error"), QStringLiteral("invalid-charge-limit")}};
    }
    adapters::windows_firmware::NativeWmiTransport transport;
    adapters::windows_battery::ChargeControl control(transport, identity());
    return charge_json(application::battery::apply_charge_limit(
        control, static_cast<application::battery::ChargeLimit>(percent)));
}
QJsonObject performance_command(const QString &value) {
    const QStringList names{QStringLiteral("balanced"), QStringLiteral("quiet"), QStringLiteral("turbo"),
                            QStringLiteral("full-speed")};
    const auto index = names.indexOf(value);
    if (index < 0) {
        return {{QStringLiteral("error"), QStringLiteral("invalid-performance-profile")}};
    }
    adapters::windows_firmware::NativeWmiTransport transport;
    adapters::windows_performance::PerformanceControl control(transport, identity());
    return performance_json(application::performance::apply_profile(
        control, static_cast<application::performance::Profile>(index + 1)));
}
} // namespace

namespace {
QJsonObject core_status() {
    auto battery = adapters::make_core_charge_control();
    auto performance = adapters::make_core_performance_control();
    const auto capability = battery->capability();
    QJsonArray limits;
    for (const auto limit : capability.allowed_limits)
        limits.append(static_cast<int>(limit));
    return {{"charge", charge_json(battery->read())},
            {"chargeCapability", static_cast<int>(capability.status)},
            {"allowedLimits", limits},
            {"performance", performance_json(performance->read())},
            {"performanceCapability", static_cast<int>(performance->capability())}};
}
QJsonObject core_charge_command(const QString &value) {
    const int limit = value.toInt();
    if (limit != 80 && limit != 100)
        return {{"error", "invalid-charge-limit"}};
    auto control = adapters::make_core_charge_control();
    return charge_json(application::battery::apply_charge_limit(
        *control, static_cast<application::battery::ChargeLimit>(limit)));
}
QJsonObject core_command(const QStringList &arguments) {
    if (arguments.size() == 1 && arguments.front() == QStringLiteral("core-status"))
        return core_status();
    if (arguments.size() != 2)
        return {{"error", "invalid-core-command"}};
    if (arguments.front() == QStringLiteral("core-charge")) {
        return core_charge_command(arguments.back());
    }
    const QStringList profiles{QStringLiteral("balanced"), QStringLiteral("quiet"), QStringLiteral("turbo")};
    const auto index = profiles.indexOf(arguments.back());
    if (arguments.front() != QStringLiteral("core-performance") || index < 0)
        return {{"error", "invalid-performance-profile"}};
    auto control = adapters::make_core_performance_control();
    return performance_json(application::performance::apply_profile(
        *control, static_cast<application::performance::Profile>(index + 1)));
}
} // namespace
QJsonObject firmware_command(const QStringList &arguments) {
    if (arguments.size() == 1 && arguments.front() == QStringLiteral("firmware-microphone"))
        return microphone_firmware_status();
    if (!arguments.isEmpty() && arguments.front().startsWith(QStringLiteral("core-")))
        return core_command(arguments);
    if (arguments.size() == 2 && arguments.front() == QStringLiteral("qualify-firmware"))
        return qualify_firmware(arguments.back());
    if (arguments.size() != 2)
        return {{QStringLiteral("error"), QStringLiteral("expected charge 80|100 or performance profile")}};
    if (arguments.front() == QStringLiteral("charge"))
        return charge_command(arguments.back());
    if (arguments.front() == QStringLiteral("performance"))
        return performance_command(arguments.back());
    return {{QStringLiteral("error"), QStringLiteral("invalid-firmware-command")}};
}
} // namespace manager::device
