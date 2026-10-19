#pragma once
#include "application/runtime/session_port.hpp"
#include <cstdint>
#include <string_view>

namespace manager::application {
enum class ConnectionHealth : std::uint8_t { SessionUnavailable, CoreUnavailable, VersionMismatch, Ready };
struct ComponentIdentity {
    std::string_view version;
    std::string_view build;
};
inline ConnectionHealth connection_health(const SessionReply &reply, ComponentIdentity expected) {
    if (!reply.connected) {
        return ConnectionHealth::SessionUnavailable;
    }
    if (!reply.daemon.connected) {
        return ConnectionHealth::CoreUnavailable;
    }
    return reply.daemon.version == expected.version && reply.daemon.build == expected.build &&
                   reply.session_version == expected.version && reply.session_build == expected.build
               ? ConnectionHealth::Ready
               : ConnectionHealth::VersionMismatch;
}
} // namespace manager::application
