#pragma once
#include "application/runtime/session_port.hpp"
#include "presentation/common/rounded_frame.hpp"
#include "presentation/common/runtime_monitor.hpp"
#include <QButtonGroup>
#include <QLabel>
#include <QPushButton>
#include <QString>
#include <QToolButton>
#include <QVBoxLayout>
#include <array>

namespace manager::presentation {
struct TrayDeviceState {
    int profileIndex{-1};
    bool profileAvailable{false};
    int refreshHz{0};
    bool refreshAvailable{false};
    bool microphoneAvailable{false};
    bool microphoneMuted{false};
    bool pending{false};
    QString status;
};

class TrayDashboard final : public RoundedFrame {
    Q_OBJECT
  public:
    explicit TrayDashboard(RuntimeMonitor &runtime);
    void toggle(const QRect &anchor);
    void retranslate();
    void setDeviceState(TrayDeviceState state);
  signals:
    void openManager();
    void profileRequested(int index);
    void refreshRequested();

  protected:
    void changeEvent(QEvent *event) override;

  private:
    RuntimeMonitor &runtime_;
    QLabel title_;
    QLabel publisher_;
    QLabel version_;
    QLabel status_;
    QLabel device_status_;
    QLabel microphone_;
    QLabel microphone_icon_;
    QToolButton refresh_;
    QLabel mode_label_;
    std::array<QToolButton, 3> modes_;
    QButtonGroup mode_group_;
    QPushButton open_;
    TrayDeviceState device_state_;
    void build_layout();

    QVBoxLayout *build_header();
    void connect_actions();
    void configure_header();
    void build_modes(QVBoxLayout &layout);
    void update_device_controls();
    QString microphone_text() const;
    void update_microphone_icon();
};
} // namespace manager::presentation
