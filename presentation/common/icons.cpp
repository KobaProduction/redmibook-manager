#include "presentation/common/icons.hpp"
#include <QIcon>
#include <QPainter>
static void initialize_symbols() {
    Q_INIT_RESOURCE(manager_symbols);
}
namespace manager::presentation {
void initialize_icons() {
    static const bool initialized = [] {
        initialize_symbols();
        return true;
    }();
    (void)initialized;
}
QPixmap symbol_pixmap(const QString &name, const QColor &color, int extent, qreal dpr) {
    initialize_icons();
    auto result = QIcon(QStringLiteral(":/icons/%1.svg").arg(name)).pixmap(QSize(extent, extent), dpr);
    QPainter painter(&result);
    painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
    painter.fillRect(result.rect(), color);
    return result;
}
} // namespace manager::presentation
