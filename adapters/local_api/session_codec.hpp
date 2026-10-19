#pragma once
#include "application/runtime/connection_health.hpp"
#include <QByteArray>

namespace manager::adapters {
QByteArray encode_session_reply(const application::DiagnosticReply &reply, std::uint32_t process_id,
                                application::ComponentIdentity identity);
application::SessionReply decode_session_reply(const QByteArray &message);
} // namespace manager::adapters
