#pragma once
#include "application/runtime/read_status.hpp"
#include "presentation/common/preferences.hpp"
#include "presentation/common/rounded_frame.hpp"
#include <QLabel>
#include <QTimer>

namespace manager::presentation {
class DiagnosticOsd final : public RoundedFrame, public application::DiagnosticFeedbackPort {
    Q_OBJECT
  public:
    explicit DiagnosticOsd(Preferences &preferences);
    void show_result(const application::DiagnosticReply &reply) override;

  protected:
    void changeEvent(QEvent *event) override;

  private:
    Preferences &preferences_;
    QLabel title_;
    QLabel detail_;
    QTimer hide_;
    application::DiagnosticReply reply_;
    void retranslate();
    void relocate();
};
} // namespace manager::presentation
