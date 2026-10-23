#pragma once

#include <cstdlib>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>

namespace manager::adapters {
[[nodiscard]] inline std::optional<std::string> environment_value(const char *name) {
#ifdef _MSC_VER
    char *value = nullptr;
    std::size_t size = 0;
    const int status = _dupenv_s(&value, &size, name);
    const std::unique_ptr<char, decltype(&std::free)> owned(value, &std::free);
    if (status != 0)
        throw std::runtime_error("Cannot read development environment");
#else
    const char *value = std::getenv(name);
#endif
    return value ? std::optional<std::string>(value) : std::nullopt;
}
} // namespace manager::adapters
