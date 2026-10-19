#pragma once
#include "application/overview/device_state.hpp"
#include <QByteArray>
#include <optional>

namespace manager::adapters {
QByteArray encode_device_state(const application::overview::DeviceState &state);
std::optional<application::overview::DeviceState> decode_device_state(const QByteArray &frame);
} // namespace manager::adapters
