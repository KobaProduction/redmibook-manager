#include "apps/device/firmware.hpp"

namespace manager::device {
QJsonObject firmware_status() {
    return {{QStringLiteral("detail"), QStringLiteral("linux-firmware-not-implemented")}};
}
QJsonObject firmware_command(const QStringList &) {
    return firmware_status();
}
} // namespace manager::device
