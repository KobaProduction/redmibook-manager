#pragma once
#include "application/nfc/port.hpp"
#include <memory>

namespace manager::adapters {
[[nodiscard]] std::shared_ptr<application::nfc::TagPort> make_nfc_tag();
} // namespace manager::adapters
