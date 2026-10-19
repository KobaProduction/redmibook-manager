#pragma once

#include <cstdint>
#include <string>

namespace manager::application {
struct DiagnosticReply {
    bool connected{false};
    std::string version;
    std::string build;
    std::uint32_t process_id{0};
};

class DiagnosticPort {
  public:
    virtual ~DiagnosticPort() = default;
    [[nodiscard]] virtual DiagnosticReply probe() const = 0;
};
} // namespace manager::application
