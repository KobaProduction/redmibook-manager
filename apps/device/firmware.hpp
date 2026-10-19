#pragma once
#include <QJsonObject>
#include <QStringList>

namespace manager::device {
QJsonObject firmware_status();
QJsonObject firmware_command(const QStringList &arguments);
} // namespace manager::device
