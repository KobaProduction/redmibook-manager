#pragma once
#include <filesystem>
#include <string>

namespace manager::adapters::windows {
std::wstring session_autostart_xml(const std::filesystem::path &executable);
int session_autostart_command(bool remove);
} // namespace manager::adapters::windows
