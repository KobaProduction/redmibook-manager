#include "presentation/common/rounded_frame.hpp"
#include "presentation/common/appearance/widget_controls.hpp"
#include <QPainter>
namespace manager::presentation {
RoundedFrame::RoundedFrame(Qt::WindowFlags flags) : QFrame(nullptr, flags) {
    setAttribute(Qt::WA_TranslucentBackground);
    set_rounded(true);
}
void RoundedFrame::set_rounded(bool enabled) {
    rounded_ = enabled;
    setStyleSheet(surface_controls_style(enabled));
    setAutoFillBackground(false);
    update();
}
void RoundedFrame::paintEvent(QPaintEvent *) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setBrush(palette().window());
    painter.setPen(palette().mid().color());
    const auto area = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    const qreal radius = rounded_ ? 8.0 : 0.0;
    painter.drawRoundedRect(area, radius, radius);
}
} // namespace manager::presentation
