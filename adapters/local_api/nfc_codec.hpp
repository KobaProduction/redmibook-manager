#pragma once
#include "application/nfc/port.hpp"
#include <QJsonObject>
#include <optional>

namespace manager::adapters::nfc_wire {
QJsonObject envelope(const char *kind);
bool compatible(const QJsonObject &object);
QJsonObject snapshot(const application::nfc::Snapshot &state);
std::optional<application::nfc::Snapshot> parse_snapshot(const QJsonObject &object);
QString bytes(const application::nfc::Bytes &value);
std::optional<application::nfc::Bytes> parse_bytes(const QJsonValue &value);
QByteArray frame(const QJsonObject &object);
} // namespace manager::adapters::nfc_wire
