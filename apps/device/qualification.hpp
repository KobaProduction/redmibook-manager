#pragma once
#include <QJsonObject>
#include <QString>

namespace manager::device {
// Explicit owner-operated qualification only; never called by runtime startup.
QJsonObject qualify_firmware(const QString &journal_path);
} // namespace manager::device
