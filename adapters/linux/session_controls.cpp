#include "adapters/platform/session_controls.hpp"

namespace manager::adapters {
application::microphone::Observation NativeMicrophone::read() const {
    return {application::microphone::Status::NotImplemented, {}, "linux-microphone-not-implemented"};
}
application::microphone::Observation NativeMicrophone::set_muted(bool) const {
    return read();
}
application::display::Snapshot NativeRefresh::read() const {
    return {application::display::Status::NotImplemented, {}, "linux-refresh-not-implemented"};
}
application::display::Change NativeRefresh::set_refresh(const std::string &, unsigned int) const {
    return {application::display::Status::NotImplemented, 0, "linux-refresh-not-implemented"};
}
} // namespace manager::adapters
