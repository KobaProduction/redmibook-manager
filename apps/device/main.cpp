#include "adapters/device_profile/native_profile.hpp"
#include "adapters/platform/session_controls.hpp"
#include "apps/device/firmware.hpp"
#include "apps/device/vendor_events.hpp"
#include <QCoreApplication>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <cstdio>

namespace {
QJsonObject microphone_json(const manager::application::microphone::Observation &value) {
    return {{QStringLiteral("confirmed"),
             value.status == manager::application::microphone::Status::Ready && value.muted.has_value()},
            {QStringLiteral("status"), static_cast<int>(value.status)},
            {QStringLiteral("detail"), QString::fromStdString(value.detail)},
            {QStringLiteral("muted"), value.muted ? QJsonValue(*value.muted) : QJsonValue()},
            {QStringLiteral("hardwareMuteSupported"),
             value.hardware_mute_supported ? QJsonValue(*value.hardware_mute_supported) : QJsonValue()}};
}
QJsonObject snapshot() {
    const auto profile = manager::adapters::NativeDeviceProfile{}.read();
    const auto screens = manager::adapters::NativeRefresh{}.read();
    QJsonArray displays;
    for (const auto &screen : screens.displays) {
        QJsonArray rates;
        for (const auto rate : screen.available_hz) {
            rates.append(static_cast<int>(rate));
        }
        displays.append(QJsonObject{{QStringLiteral("id"), QString::fromStdString(screen.id)},
                                    {QStringLiteral("hz"), static_cast<int>(screen.current_hz)},
                                    {QStringLiteral("internalPanel"), screen.internal_panel},
                                    {QStringLiteral("rates"), rates}});
    }
    return {{QStringLiteral("model"), QString::fromStdString(profile.model)},
            {QStringLiteral("board"), QString::fromStdString(profile.board_model)},
            {QStringLiteral("bios"), QString::fromStdString(profile.bios_version)},
            {QStringLiteral("microphone"), microphone_json(manager::adapters::NativeMicrophone{}.read())},
            {QStringLiteral("displays"), displays},
            {QStringLiteral("firmware"), manager::device::firmware_status()}};
}
QJsonObject microphone_command(const QString &action) {
    const manager::adapters::NativeMicrophone control;
    if (action == QStringLiteral("toggle")) {
        return microphone_json(manager::application::microphone::toggle(control));
    }
    if (action == QStringLiteral("mute") || action == QStringLiteral("unmute")) {
        return microphone_json(control.set_muted(action == QStringLiteral("mute")));
    }
    return {{QStringLiteral("error"), QStringLiteral("invalid-microphone-action")}};
}
QJsonObject refresh_command(const QStringList &args) {
    bool valid{};
    const auto hz = args.back().toUInt(&valid);
    if (!valid) {
        return {{QStringLiteral("error"), QStringLiteral("invalid-refresh-rate")}};
    }
    const auto value = manager::adapters::NativeRefresh{}.set_refresh(args[1].toStdString(), hz);
    return {{QStringLiteral("confirmed"), value.status == manager::application::display::Status::Ready},
            {QStringLiteral("status"), static_cast<int>(value.status)},
            {QStringLiteral("detail"), QString::fromStdString(value.detail)},
            {QStringLiteral("hz"), static_cast<int>(value.observed_hz)}};
}
bool write_output(const QJsonObject &result, const QString &path) {
    const auto json = QJsonDocument(result).toJson();
    if (!path.isEmpty()) {
        QFile output(path);
        if (!output.open(QIODevice::WriteOnly) || output.write(json) != json.size()) {
            return false;
        }
    }
    std::fwrite(json.constData(), 1, static_cast<std::size_t>(json.size()), stdout);
    return true;
}

QJsonObject execute(const QStringList &args) {
    if (args.size() == 2 && args.front() == QStringLiteral("vendor-events")) {
        bool valid{};
        const int duration = args.back().toInt(&valid);
        if (!valid || duration < 1 || duration > 60000) {
            return {{QStringLiteral("error"), QStringLiteral("duration-must-be-1-to-60000-ms")}};
        }
        return manager::device::vendor_events(static_cast<unsigned int>(duration));
    }
    if (args.isEmpty() || args.front() == QStringLiteral("status")) {
        return snapshot();
    }
    if (args.size() == 2 && args.front() == QStringLiteral("microphone")) {
        return microphone_command(args.back());
    }
    if (args.size() == 3 && args.front() == QStringLiteral("refresh")) {
        return refresh_command(args);
    }
    return manager::device::firmware_command(args);
}
} // namespace
int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);
    auto args = app.arguments().mid(1);
    QString output_path;
    const auto output_index = args.indexOf(QStringLiteral("--output"));
    if (output_index >= 0 && output_index + 1 < args.size()) {
        output_path = args.takeAt(output_index + 1);
        args.removeAt(output_index);
    }
    const auto result = execute(args);
    if (!write_output(result, output_path) || result.contains(QStringLiteral("error"))) {
        return 2;
    }
    const bool inventory = args.isEmpty() || args.front() == QStringLiteral("status") ||
                           args.front() == QStringLiteral("core-status") ||
                           args.front() == QStringLiteral("vendor-events");
    return inventory || result.value(QStringLiteral("confirmed")).toBool() ? 0 : 1;
}
