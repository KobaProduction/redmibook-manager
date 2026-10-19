#include "presentation/osd/placement.hpp"
#include "application/ui_preferences/preferences.hpp"
#include <algorithm>
#include <array>

namespace manager::presentation {
QPoint feedback_position(const QRect &area, const QSize &size, std::string_view position) {
    const auto &positions = application::osd_positions;
    const auto found = std::find(positions.begin(), positions.end(), position);
    const auto index = found == positions.end() ? 0 : static_cast<std::size_t>(found - positions.begin());
    const int max_x = std::max(area.left(), area.right() - size.width() + 1);
    const int max_y = std::max(area.top(), area.bottom() - size.height() + 1);
    const std::array<int, 3> xs{area.left() + 24, area.left() + (area.width() - size.width()) / 2,
                                max_x - 24};
    const std::array<int, 3> ys{area.top() + 24, area.top() + (area.height() - size.height()) / 2,
                                max_y - 24};
    return {std::clamp(xs[index % 3], area.left(), max_x), std::clamp(ys[index / 3], area.top(), max_y)};
}
} // namespace manager::presentation
