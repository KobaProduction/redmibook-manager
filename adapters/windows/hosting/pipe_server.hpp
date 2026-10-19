#pragma once
#include <windows.h>

namespace manager::adapters::windows {
int serve_diagnostics(HANDLE stop, const wchar_t *name);
}
