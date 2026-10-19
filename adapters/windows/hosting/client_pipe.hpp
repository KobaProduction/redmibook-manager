#pragma once
#include "application/runtime/diagnostic_port.hpp"
#include <windows.h>
namespace manager::adapters::windows {
bool trusted_machine_pipe(HANDLE pipe, DWORD &pid);
[[nodiscard]] application::DiagnosticReply probe_daemon();
} // namespace manager::adapters::windows
