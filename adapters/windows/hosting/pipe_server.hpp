#pragma once
#include <functional>
#include <string>
#include <string_view>
#include <windows.h>

namespace manager::adapters::windows {
using PipeHandler = std::function<std::string(HANDLE, std::string_view)>;
int serve_requests(HANDLE stop, const wchar_t *name, const PipeHandler &handler);
int serve_diagnostics(HANDLE stop, const wchar_t *name);
} // namespace manager::adapters::windows
