#pragma once
#include <QColor>
#include <QPixmap>
#include <QString>
namespace manager::presentation {
void initialize_icons();
QPixmap symbol_pixmap(const QString &name, const QColor &color, int extent, qreal dpr);
} // namespace manager::presentation
