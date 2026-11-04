#include "adapters/local_api/nfc_codec.hpp"
#include "application/runtime/build_identity.hpp"
#include <QJsonDocument>

namespace manager::adapters::nfc_wire {
QJsonObject envelope(const char *kind) {
    return {{"schema", 1},
            {"kind", QString::fromLatin1(kind)},
            {"version", application::build_identity::version},
            {"build", application::build_identity::payload}};
}
bool compatible(const QJsonObject &object) {
    return object.value("schema").toInt() == 1 &&
           object.value("version").toString() == QLatin1String(application::build_identity::version) &&
           object.value("build").toString() == QLatin1String(application::build_identity::payload);
}
QString bytes(const application::nfc::Bytes &value) {
    return QString::fromLatin1(
        QByteArray(reinterpret_cast<const char *>(value.data()), static_cast<qsizetype>(value.size()))
            .toBase64());
}
std::optional<application::nfc::Bytes> parse_bytes(const QJsonValue &value) {
    if (!value.isString() || value.toString().size() > 2800)
        return {};
    const auto decoded =
        QByteArray::fromBase64Encoding(value.toString().toLatin1(), QByteArray::AbortOnBase64DecodingErrors);
    if (!decoded || decoded.decoded.size() > 2048)
        return {};
    return application::nfc::Bytes(decoded.decoded.begin(), decoded.decoded.end());
}
QJsonObject snapshot(const application::nfc::Snapshot &state) {
    return {{"status", static_cast<int>(state.status)},
            {"present", state.present},
            {"name", QString::fromStdString(state.name)},
            {"uid", QString::fromStdString(state.uid)},
            {"field", state.field ? QJsonValue(*state.field) : QJsonValue()},
            {"capacity", static_cast<int>(state.capacity)},
            {"maxContent", static_cast<int>(state.max_content_size)},
            {"writable", state.writable},
            {"eventDriven", state.event_driven},
            {"content", bytes(state.content)}};
}
std::optional<application::nfc::Snapshot> parse_snapshot(const QJsonObject &object) {
    const int status = object.value("status").toInt(-1);
    const auto content = parse_bytes(object.value("content"));
    if (status < 0 || status > static_cast<int>(application::nfc::Status::DriverMissing) || !content ||
        !object.value("present").isBool() || !object.value("writable").isBool() ||
        !object.value("eventDriven").isBool())
        return {};
    application::nfc::Snapshot state{
        .status = static_cast<application::nfc::Status>(status),
        .present = object.value("present").toBool(),
        .name = object.value("name").toString().left(128).toStdString(),
        .uid = object.value("uid").toString().left(64).toStdString(),
        .field =
            object.value("field").isBool() ? std::optional{object.value("field").toBool()} : std::nullopt,
        .capacity = static_cast<std::uint32_t>(object.value("capacity").toInt(0)),
        .max_content_size = static_cast<std::uint32_t>(object.value("maxContent").toInt(0)),
        .writable = object.value("writable").toBool(),
        .event_driven = object.value("eventDriven").toBool(),
        .content = *content};
    return state.capacity <= 2048 && state.max_content_size <= state.capacity ? std::optional{state}
                                                                              : std::nullopt;
}
QByteArray frame(const QJsonObject &object) {
    return QJsonDocument(object).toJson(QJsonDocument::Compact) + '\n';
}
} // namespace manager::adapters::nfc_wire
