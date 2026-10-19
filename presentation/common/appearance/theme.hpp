#pragma once
#include <QObject>
#include <QPalette>
#include <QString>

namespace manager::presentation {
class ThemeController final : public QObject {
    Q_OBJECT
  public:
    ThemeController();
    void set_mode(const QString &mode);

  signals:
    void changed();

  private:
    QPalette system_palette_;
    QString mode_{QStringLiteral("system")};
    bool applying_{false};
    bool pending_{false};
    void schedule_palette();
    bool eventFilter(QObject *watched, QEvent *event) override;
    void apply_palette();
};
} // namespace manager::presentation
