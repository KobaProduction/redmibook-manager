#pragma once
#include "application/runtime/session_port.hpp"
#include "presentation/common/rounded_frame.hpp"
#include "presentation/common/runtime_monitor.hpp"
#include <QButtonGroup>
#include <QLabel>
#include <QPushButton>
#include <QToolButton>
#include <QVBoxLayout>
#include <array>

namespace manager::presentation {
class TrayDashboard final : public RoundedFrame {
    Q_OBJECT
  public:
    explicit TrayDashboard(RuntimeMonitor &runtime);
    void toggle(const QRect &anchor);
    void retranslate();
  signals:
    void openManager();

  protected:
    void changeEvent(QEvent *event) override;

  private:
    RuntimeMonitor &runtime_;
    QLabel title_;
    QLabel publisher_;
    QLabel version_;
    QLabel status_;
    QLabel mock_notice_;
    QLabel mode_label_;
    std::array<QToolButton, 3> modes_;
    QButtonGroup mode_group_;
    QPushButton open_;
    void build_layout();

    QVBoxLayout *build_header();
    void connect_actions();
    void configure_header();
    void build_modes(QVBoxLayout &layout);
};
} // namespace manager::presentation
