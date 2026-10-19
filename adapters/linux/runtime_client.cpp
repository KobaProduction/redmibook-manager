#include "adapters/platform/runtime.hpp"
#include <cstdlib>
#include <unistd.h>

namespace manager::adapters {
application::DiagnosticReply NativeDiagnostic::probe() const {
    return {};
}

std::string session_identity() {
    const auto *session = std::getenv("XDG_SESSION_ID");
    return std::to_string(getuid()) + ":" + (session == nullptr ? "desktop" : session);
}

void register_application_restart() {}

void permit_foreground_activation(std::int64_t) {}
bool restart_machine_runtime() {
    return false;
}
int run_machine_host(int, char *[]) {
    return 2;
}
} // namespace manager::adapters
