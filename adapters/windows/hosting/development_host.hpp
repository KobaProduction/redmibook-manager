#pragma once

#include "adapters/platform/development_scope.hpp"
#include "adapters/windows/hosting/handle.hpp"
#include "adapters/windows/hosting/pipe_server.hpp"
#include <charconv>
#include <string>

namespace manager::adapters::windows {
[[nodiscard]] inline std::wstring diagnostic_endpoint() {
    const auto scope = development_scope();
    return scope.empty() ? std::wstring(pipe_name)
                         : std::wstring(pipe_name) + L".Dev." + std::wstring(scope.begin(), scope.end());
}

[[nodiscard]] inline bool development_server(DWORD pid) {
    const auto value = environment_value("MANAGER_DEV_CORE_PID");
    if (!value || development_scope().empty()) {
        return false;
    }
    const auto &text = *value;
    DWORD expected = 0;
    const auto result = std::from_chars(text.data(), text.data() + text.size(), expected);
    return result.ec == std::errc{} && result.ptr == text.data() + text.size() && expected != 0 &&
           expected == pid;
}

// Development hosts deliberately expose only PING. Future hardware dispatch must not be added here.
inline int run_development_host() {
    if (development_scope().empty()) {
        return 2;
    }
    const Handle stop(CreateEventW(nullptr, TRUE, FALSE, nullptr));
    return stop.valid() ? serve_diagnostics(stop.get(), diagnostic_endpoint().c_str()) : 1;
}
} // namespace manager::adapters::windows
