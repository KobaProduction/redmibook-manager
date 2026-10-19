#pragma once

#include "application/runtime/diagnostic_port.hpp"
#include <cstdint>
#include <string>

namespace manager::adapters {
class NativeDiagnostic final : public application::DiagnosticPort {
  public:
    [[nodiscard]] application::DiagnosticReply probe() const override;
};

[[nodiscard]] std::string session_identity();
void register_application_restart();
void permit_foreground_activation(std::int64_t process_id);
[[nodiscard]] bool restart_machine_runtime();
int run_machine_host(int argc, char *argv[]);
} // namespace manager::adapters
