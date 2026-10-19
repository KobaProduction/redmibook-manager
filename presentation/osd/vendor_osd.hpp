#pragma once
#include "application/display/refresh_port.hpp"
#include "presentation/common/preferences.hpp"
#include "presentation/common/rounded_frame.hpp"
#include <QLabel>
#include <QMetaObject>
#include <QTimer>
#include <cstdint>
#include <vector>

class QScreen;

namespace manager::presentation {
class VendorOsd final : public RoundedFrame {
    Q_OBJECT
  public:
    explicit VendorOsd(Preferences &preferences);
    void present_mi();
    void present_profile(std::uint8_t announced_profile);
    void prepare_refresh();
    void present_refresh(const application::display::Change &result);

  private:
    enum class Kind : std::uint8_t { Mi, Profile, Refresh };
    Preferences &preferences_;
    QLabel text_;
    QTimer timeout_;
    QTimer refresh_settle_;
    std::vector<QMetaObject::Connection> refresh_connections_;
    bool awaiting_refresh_{false};
    bool refresh_result_ready_{false};
    unsigned int qt_observed_hz_{};
    Kind kind_{Kind::Mi};
    std::uint8_t profile_{};
    application::display::Change refresh_;
    void retranslate();
    void reveal();
    void watch_refresh_screen(QScreen &screen);
    void finish_refresh();
    void clear_refresh_connections();
};
} // namespace manager::presentation
