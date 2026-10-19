#pragma once
#include <QFrame>
namespace manager::presentation {
class RoundedFrame : public QFrame {
  public:
    explicit RoundedFrame(Qt::WindowFlags flags);
    void set_rounded(bool enabled);

  protected:
    void paintEvent(QPaintEvent *event) override;

  private:
    bool rounded_{true};
};
} // namespace manager::presentation
