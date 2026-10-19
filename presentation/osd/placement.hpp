#pragma once
#include <QPoint>
#include <QRect>
#include <QSize>
#include <string_view>

namespace manager::presentation {
[[nodiscard]] QPoint feedback_position(const QRect &area, const QSize &size, std::string_view position);
}
