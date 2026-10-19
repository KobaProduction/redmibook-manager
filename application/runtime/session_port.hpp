#pragma once
#include "application/runtime/diagnostic_port.hpp"
#include <functional>
#include <string_view>

namespace manager::application {
enum class SessionCommand : std::uint8_t { Status, Diagnostic, Restart, Shutdown, OpenTray };
struct SessionReply {
    bool connected{false};
    DiagnosticReply daemon;
    std::uint32_t process_id{0};
    std::string session_version;
    std::string session_build;
};

class SessionPort {
  public:
    virtual ~SessionPort() = default;
    // One outstanding bounded request; completion on the caller's event thread.
    virtual bool request(SessionCommand command, std::function<void(SessionReply)> completion) = 0;
};
} // namespace manager::application
