#pragma once

#include <QJsonObject>

namespace manager::device {

// Explicit passive diagnostic command only; invoke from CLI/worker, never the GUI thread.
[[nodiscard]] QJsonObject vendor_events(unsigned int timeout_ms);

} // namespace manager::device
