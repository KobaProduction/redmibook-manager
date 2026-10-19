#pragma once
#include "application/updates/update_port.hpp"
#include "presentation/common/preferences.hpp"
#include "presentation/common/rounded_frame.hpp"
#include <QLabel>
#include <QPushButton>
#include <QTimer>
namespace manager::presentation {
class UpdateNotice final : public RoundedFrame {
    Q_OBJECT
  public:
    explicit UpdateNotice(Preferences &preferences);
    void show_result(const application::UpdateResult &result);
  signals:
    void downloadRequested(const QString &url);

  private:
    void retranslate();
    Preferences &preferences_;
    QLabel title_;
    QPushButton download_;
    QPushButton dismiss_;
    QTimer hide_;
    QString url_;
    QString version_;
};
} // namespace manager::presentation
