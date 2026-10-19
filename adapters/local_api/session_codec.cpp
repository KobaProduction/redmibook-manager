#include "adapters/local_api/session_codec.hpp"
#include <QString>

#include <QList>
namespace manager::adapters {
QByteArray encode_session_reply(const application::DiagnosticReply &reply, std::uint32_t process_id,
                                application::ComponentIdentity identity) {
    return QStringLiteral("RMB/1 OK %1 %2 %3 %4 %5 %6 %7\n")
        .arg(process_id)
        .arg(reply.connected ? QStringLiteral("up") : QStringLiteral("down"))
        .arg(reply.process_id)
        .arg(QString::fromStdString(reply.version.empty() ? "-" : reply.version))
        .arg(QString::fromStdString(reply.build.empty() ? "-" : reply.build))
        .arg(QString::fromUtf8(identity.version.data(), static_cast<qsizetype>(identity.version.size())))
        .arg(QString::fromUtf8(identity.build.data(), static_cast<qsizetype>(identity.build.size())))
        .toUtf8();
}

application::SessionReply decode_session_reply(const QByteArray &message) {
    const auto parts = message.trimmed().split(' ');
    if ((parts.size() != 7 && parts.size() != 9) || parts[0] != "RMB/1" || parts[1] != "OK" ||
        (parts[3] != "up" && parts[3] != "down")) {
        return {};
    }
    bool session_pid_valid = false;
    bool core_pid_valid = false;
    application::SessionReply result;
    result.process_id = parts[2].toUInt(&session_pid_valid);
    const auto core_pid = parts[4].toUInt(&core_pid_valid);
    if (!session_pid_valid || !core_pid_valid || result.process_id == 0) {
        return {};
    }
    result.connected = true;
    result.daemon = {parts[3] == "up", parts[5].toStdString(), parts[6].toStdString(), core_pid};
    if (parts.size() == 9) {
        result.session_version = parts[7].toStdString();
        result.session_build = parts[8].toStdString();
    }
    return result;
}
} // namespace manager::adapters
