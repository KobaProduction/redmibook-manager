#include "adapters/local_api/device_state_codec.hpp"
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace manager::adapters {
namespace {
namespace app = application;
template <typename T> T status(const QJsonObject &object, int maximum, T unavailable) {
    const int value = object.value(QStringLiteral("status")).toInt(-1);
    return value >= 0 && value <= maximum ? static_cast<T>(value) : unavailable;
}
QJsonArray numbers(const std::vector<unsigned int> &values) {
    QJsonArray result;
    for (const auto value : values)
        result.append(static_cast<int>(value));
    return result;
}
QJsonObject microphone_json(const app::microphone::Observation &value) {
    return {{"status", static_cast<int>(value.status)},
            {"muted", value.muted ? QJsonValue(*value.muted) : QJsonValue()},
            {"hardwareMute",
             value.hardware_mute_supported ? QJsonValue(*value.hardware_mute_supported) : QJsonValue()}};
}
QJsonObject display_json(const app::display::Snapshot &value) {
    QJsonArray items;
    for (const auto &display : value.displays)
        items.append(QJsonObject{{"id", QString::fromStdString(display.id)},
                                 {"name", QString::fromStdString(display.name)},
                                 {"hz", static_cast<int>(display.current_hz)},
                                 {"internal", display.internal_panel},
                                 {"rates", numbers(display.available_hz)}});
    return {{"status", static_cast<int>(value.status)}, {"items", items}};
}
QJsonObject battery_json(const app::overview::DeviceState &state) {
    QJsonArray limits;
    for (const auto limit : state.battery_capability.allowed_limits)
        limits.append(static_cast<int>(limit));
    return {
        {"status", static_cast<int>(state.battery.status)},
        {"limit", state.battery.limit ? QJsonValue(static_cast<int>(*state.battery.limit)) : QJsonValue()},
        {"capability", static_cast<int>(state.battery_capability.status)},
        {"limits", limits}};
}
app::microphone::Observation parse_microphone(const QJsonObject &object) {
    app::microphone::Observation value;
    value.status = status(object, 4, app::microphone::Status::Unavailable);
    if (object.value("muted").isBool())
        value.muted = object.value("muted").toBool();
    if (object.value("hardwareMute").isBool())
        value.hardware_mute_supported = object.value("hardwareMute").toBool();
    return value;
}
app::display::Display parse_display(const QJsonObject &object) {
    app::display::Display value;
    value.id = object.value("id").toString().toStdString();
    value.name = object.value("name").toString().toStdString();
    const int hz = object.value("hz").toInt();
    value.current_hz = hz >= 2 && hz <= 1000 ? static_cast<unsigned int>(hz) : 0;
    value.internal_panel = object.value("internal").toBool();
    for (const auto item : object.value("rates").toArray()) {
        const int rate = item.toInt();
        if (rate >= 2 && rate <= 1000)
            value.available_hz.push_back(static_cast<unsigned int>(rate));
    }
    return value;
}
void parse_battery(const QJsonObject &object, app::overview::DeviceState &state) {
    state.battery.status = status(object, 10, app::battery::Status::Unavailable);
    const int limit = object.value("limit").toInt();
    if (limit == 80 || limit == 100)
        state.battery.limit = static_cast<app::battery::ChargeLimit>(limit);
    const int capability = object.value("capability").toInt(-1);
    if (capability >= 0 && capability <= 10)
        state.battery_capability.status = static_cast<app::battery::Status>(capability);
    for (const auto item : object.value("limits").toArray()) {
        const int allowed = item.toInt();
        if (allowed == 80 || allowed == 100)
            state.battery_capability.allowed_limits.push_back(
                static_cast<app::battery::ChargeLimit>(allowed));
    }
}
void parse_performance(const QJsonObject &object, app::overview::DeviceState &state) {
    state.performance.status = status(object, 10, app::performance::Status::Unavailable);
    const int profile = object.value("profile").toInt();
    if (profile >= 1 && profile <= 4)
        state.performance.profile = static_cast<app::performance::Profile>(profile);
    const int capability = object.value("capability").toInt(-1);
    if (capability >= 0 && capability <= 10)
        state.performance_capability = static_cast<app::performance::Status>(capability);
}
} // namespace
QByteArray encode_device_state(const application::overview::DeviceState &state) {
    const QJsonObject profile{{"status", static_cast<int>(state.performance.status)},
                              {"profile", state.performance.profile
                                              ? QJsonValue(static_cast<int>(*state.performance.profile))
                                              : QJsonValue()},
                              {"capability", static_cast<int>(state.performance_capability)}};
    const QJsonObject object{{"schema", 1},
                             {"version", MANAGER_APP_VERSION},
                             {"build", MANAGER_BUILD_ID},
                             {"revision", QString::number(state.revision)},
                             {"microphone", microphone_json(state.microphone)},
                             {"display", display_json(state.displays)},
                             {"battery", battery_json(state)},
                             {"performance", profile}};
    return QJsonDocument(object).toJson(QJsonDocument::Compact) + '\n';
}
std::optional<application::overview::DeviceState> decode_state_object(const QJsonObject &object) {
    application::overview::DeviceState state;
    state.revision = object.value("revision").toString().toULongLong();
    state.microphone = parse_microphone(object.value("microphone").toObject());
    const auto displays = object.value("display").toObject();
    state.displays.status = status(displays, 6, application::display::Status::Unavailable);
    const auto items = displays.value("items").toArray();
    if (items.size() > 32)
        return {};
    for (const auto item : items)
        state.displays.displays.push_back(parse_display(item.toObject()));
    parse_battery(object.value("battery").toObject(), state);
    parse_performance(object.value("performance").toObject(), state);
    return state;
}

std::optional<application::overview::DeviceState> decode_device_state(const QByteArray &frame) {
    if (frame.size() > 65536)
        return {};
    const auto object = QJsonDocument::fromJson(frame).object();
    if (object.value("schema").toInt() != 1 ||
        object.value("version").toString() != QLatin1String(MANAGER_APP_VERSION) ||
        object.value("build").toString() != QLatin1String(MANAGER_BUILD_ID))
        return {};
    return decode_state_object(object);
}
} // namespace manager::adapters
