#pragma once
#include "application/runtime/diagnostic_port.hpp"
namespace manager::adapters::windows {
[[nodiscard]] application::DiagnosticReply probe_daemon();
}
