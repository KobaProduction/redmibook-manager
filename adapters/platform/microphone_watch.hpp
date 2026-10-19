#pragma once
#include "application/microphone/change_source.hpp"
#include <functional>

namespace manager::adapters {
// Notification only; callers query the microphone port asynchronously for state.
std::unique_ptr<application::microphone::ChangeSubscription>
watch_native_microphone(std::function<void()> changed);
} // namespace manager::adapters
