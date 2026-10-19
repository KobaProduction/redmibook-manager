#pragma once
#include <QObject>
#include <QPoint>
#include <QScreen>
#include <QVariantList>
#include <QWindow>

namespace manager::presentation {
QString display_id(const QScreen &screen);
QScreen *resolve_display(const QString &id, const QPoint &anchor);
QRect centered_window(const QRect &area, const QSize &size);
void present_window(QWindow &window, const QPoint &anchor);
class DisplayCatalog final : public QObject {
    Q_OBJECT
  public:
    DisplayCatalog();
    QVariantList screens() const;
  signals:
    void changed();

  private:
    void observe(QScreen *screen);
};
} // namespace manager::presentation
