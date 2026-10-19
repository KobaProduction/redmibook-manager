#include "presentation/osd/vendor_osd.hpp"
#include "presentation/common/displays.hpp"
#include "presentation/osd/placement.hpp"
#include <QCursor>
#include <QGuiApplication>
#include <QScreen>
#include <QVBoxLayout>

namespace manager::presentation {
VendorOsd::VendorOsd(Preferences &preferences)
    : RoundedFrame(Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint), preferences_(preferences) {
    setAttribute(Qt::WA_ShowWithoutActivating);
    set_rounded(preferences_.rounded_corners());
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 18, 24, 18);
    layout->addWidget(&text_);
    timeout_.setSingleShot(true);
    refresh_settle_.setSingleShot(true);
    // Bounded fallback: some platform plugins do not publish an accurate Qt refresh signal.
    refresh_settle_.setInterval(1000);
    connect(&refresh_settle_, &QTimer::timeout, this, &VendorOsd::finish_refresh);
    connect(&timeout_, &QTimer::timeout, this, &QWidget::hide);
    connect(&preferences_, &Preferences::languageChanged, this, &VendorOsd::retranslate);
    connect(&preferences_, &Preferences::changed, this,
            [this] { set_rounded(preferences_.rounded_corners()); });
}
void VendorOsd::present_mi() {
    kind_ = Kind::Mi;
    reveal();
}
void VendorOsd::present_profile(std::uint8_t announced_profile) {
    kind_ = Kind::Profile;
    profile_ = announced_profile;
    reveal();
}
void VendorOsd::clear_refresh_connections() {
    for (const auto &connection : refresh_connections_)
        QObject::disconnect(connection);
    refresh_connections_.clear();
}
void VendorOsd::prepare_refresh() {
    clear_refresh_connections();
    refresh_settle_.stop();
    awaiting_refresh_ = true;
    refresh_result_ready_ = false;
    qt_observed_hz_ = 0;
    hide();
    for (auto *screen : QGuiApplication::screens())
        watch_refresh_screen(*screen);
    refresh_connections_.push_back(connect(qGuiApp, &QGuiApplication::screenAdded, this,
                                           [this](QScreen *screen) { watch_refresh_screen(*screen); }));
}
void VendorOsd::watch_refresh_screen(QScreen &screen) {
    refresh_connections_.push_back(connect(&screen, &QScreen::refreshRateChanged, this, [this](qreal hz) {
        if (hz < 2 || hz > 1000)
            return;
        qt_observed_hz_ = static_cast<unsigned int>(qRound(hz));
        if (awaiting_refresh_ && refresh_result_ready_ && qt_observed_hz_ == refresh_.observed_hz)
            finish_refresh();
    }));
}
void VendorOsd::finish_refresh() {
    awaiting_refresh_ = false;
    refresh_result_ready_ = false;
    refresh_settle_.stop();
    clear_refresh_connections();
    kind_ = Kind::Refresh;
    reveal();
}

void VendorOsd::present_refresh(const application::display::Change &result) {
    refresh_ = result;
    refresh_result_ready_ = true;
    if (result.status != application::display::Status::Ready || !awaiting_refresh_ ||
        qt_observed_hz_ == result.observed_hz) {
        finish_refresh();
        return;
    }
    refresh_settle_.start();
}
void VendorOsd::retranslate() {
    if (kind_ == Kind::Mi) {
        text_.setText(tr("Mi button detected"));
    } else if (kind_ == Kind::Profile) {
        QString name;
        switch (profile_) {
        case 1:
            name = tr("Balanced");
            break;
        case 2:
            name = tr("Quiet");
            break;
        case 3:
            name = tr("Turbo");
            break;
        default:
            name = tr("Unknown");
            break;
        }
        text_.setText(tr("Firmware profile: %1").arg(name));
    } else {
        text_.setText(refresh_.status == application::display::Status::Ready
                          ? tr("Display refresh rate: %1 Hz").arg(refresh_.observed_hz)
                          : tr("Display refresh change could not be confirmed"));
    }
}
void VendorOsd::reveal() {
    preferences_.refresh();
    retranslate();
    adjustSize();
    if (const auto *screen = resolve_display(preferences_.osd_screen(), QCursor::pos()))
        move(feedback_position(screen->availableGeometry(), size(),
                               preferences_.osd_position().toStdString()));
    show();
    raise();
    timeout_.start(2200);
}
} // namespace manager::presentation
