#include "adapters/platform/microphone_watch.hpp"
namespace manager::adapters {
std::unique_ptr<application::microphone::ChangeSubscription>
watch_native_microphone(const std::function<void()> &) {
    return {};
}
} // namespace manager::adapters
