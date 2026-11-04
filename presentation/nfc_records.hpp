#pragma once
#include "application/nfc/port.hpp"
#include <QVariantList>

namespace manager::presentation {
QVariantList nfcRecords(const application::nfc::Bytes &content);
} // namespace manager::presentation
