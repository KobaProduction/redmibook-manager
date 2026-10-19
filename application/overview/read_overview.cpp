#include "application/overview/read_overview.hpp"

namespace manager::application {
ReadOverview::ReadOverview(const HostInfoPort &host) : host_(host) {}

Overview ReadOverview::execute() const {
    return {.greeting = "Hello, World!", .platform = host_.platform()};
}
} // namespace manager::application
