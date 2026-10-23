#pragma once
#include <windows.h>
namespace manager::adapters::windows_battery {
// MinGW 13 omits these public powrprof.h SDK declarations. Documented Win32 ABI:
// https://learn.microsoft.com/windows/win32/api/powrprof/ns-powrprof-device_notify_subscribe_parameters
struct NotifyParameters {
    ULONG(CALLBACK *callback)(PVOID, ULONG, PVOID);
    PVOID context;
};
inline constexpr DWORD callback_notification = 2;
} // namespace manager::adapters::windows_battery
