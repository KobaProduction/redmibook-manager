#pragma once
#include "application/nfc/files.hpp"

namespace manager::adapters {
std::shared_ptr<application::nfc::FilePort> make_nfc_files();
} // namespace manager::adapters
