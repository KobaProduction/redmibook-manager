#pragma once
#include "application/display/refresh_port.hpp"
#include "application/microphone/control_port.hpp"

namespace manager::adapters {
class NativeMicrophone final : public application::microphone::ControlPort {
  public:
    application::microphone::Observation read() const override;
    application::microphone::Observation set_muted(bool muted) const override;
};
class NativeRefresh final : public application::display::RefreshPort {
  public:
    application::display::Snapshot read() const override;
    application::display::Change set_refresh(const std::string &id, unsigned int hz) const override;
};
} // namespace manager::adapters
