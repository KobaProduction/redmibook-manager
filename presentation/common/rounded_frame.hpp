#pragma once
#include <QFrame>
namespace manager::presentation {
class RoundedFrame : public QFrame {
  public:
    explicit RoundedFrame(Qt::WindowFlags flags);
    void set_rounded(bool enabled);
    void set_emphasized(bool enabled) {
        emphasized_ = enabled;
        update();
    }

  protected:
    void paintEvent(QPaintEvent *event) override;

  private:
    bool rounded_{true};
    bool emphasized_{false};
};
} // namespace manager::presentation
