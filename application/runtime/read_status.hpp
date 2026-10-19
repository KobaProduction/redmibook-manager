#pragma once
#include "application/runtime/diagnostic_port.hpp"

namespace manager::application {
class ReadRuntimeStatus final {
  public:
    explicit ReadRuntimeStatus(const DiagnosticPort &port) : port_(port) {}
    [[nodiscard]] DiagnosticReply execute() const {
        return port_.probe();
    }

  private:
    const DiagnosticPort &port_;
};

class DiagnosticFeedbackPort {
  public:
    virtual ~DiagnosticFeedbackPort() = default;
    virtual void show_result(const DiagnosticReply &reply) = 0;
};
} // namespace manager::application
