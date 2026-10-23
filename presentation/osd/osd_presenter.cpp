#include "presentation/osd/osd_presenter.hpp"
#include "presentation/common/displays.hpp"
#include "presentation/common/icons.hpp"
#include "presentation/osd/placement.hpp"
#include <QApplication>
#include <QCursor>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QScreen>

namespace manager::presentation {
OsdPresenter::OsdPresenter(Preferences &preferences)
    : RoundedFrame(Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint), preferences_(preferences) {
    setAttribute(Qt::WA_ShowWithoutActivating);
    set_emphasized(true);
    set_rounded(preferences_.rounded_corners());
    static_cast<void>(winId());
    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(24, 18, 24, 18);
    layout->setSpacing(12);
    icon_.setObjectName(QStringLiteral("osdIcon"));
    text_.setObjectName(QStringLiteral("osdText"));
    layout->addWidget(&icon_, 0, Qt::AlignVCenter);
    layout->addWidget(&text_);
    timeout_.setSingleShot(true);
    connect(&timeout_, &QTimer::timeout, this, &QWidget::hide);
    connect(&preferences_, &Preferences::languageChanged, this, &OsdPresenter::retranslate);
    connect(&preferences_, &Preferences::changed, this, [this] {
        set_rounded(preferences_.rounded_corners());
        if (!preferences_.osd_enabled())
            hide();
    });
}
void OsdPresenter::present_mi() {
    kind_ = Kind::Mi;
    reveal();
}
void OsdPresenter::present_profile(std::uint8_t announced_profile) {
    (void)present(application::feedback::Event{application::feedback::Kind::Performance, announced_profile});
}
void OsdPresenter::prepare_refresh() {
    hide();
}
void OsdPresenter::present_refresh(const application::display::Change &result) {
    if (result.status == application::display::Status::Ready &&
        !distinct_.accept({application::feedback::Kind::Refresh, result.observed_hz}))
        return;
    if (result.status != application::display::Status::Ready)
        distinct_.forget(application::feedback::Kind::Refresh);
    // The adapter already read the applied mode. A second Qt screen notification is not required.
    refresh_ = result;
    kind_ = Kind::Refresh;
    reveal();
}
QString OsdPresenter::profile_text() const {
    QString name;
    switch (profile_) {
    case 1:
        name = tr("Balanced");
        break;
    case 2:
        name = tr("Quiet");
        break;
    case 3:
        name = tr("Performance");
        break;
    default:
        name = tr("Unknown");
        break;
    }
    return name;
}
void OsdPresenter::retranslate() {
    if (kind_ == Kind::Mi) {
        text_.setText(tr("Mi button detected"));
    } else if (kind_ == Kind::Profile) {
        text_.setText(tr("Firmware profile: %1").arg(profile_text()));
    } else if (kind_ == Kind::Microphone) {
        const bool known =
            microphone_.status == application::microphone::Status::Ready && microphone_.muted.has_value();
        text_.setText(known ? (*microphone_.muted ? tr("Microphone muted") : tr("Microphone active"))
                            : tr("Microphone change could not be confirmed"));
    } else if (kind_ == Kind::Diagnostic) {
        text_.setText(tr("Diagnostic signal received") + '\n' +
                      (diagnostic_.connected ? tr("Daemon responded. Test only — no hardware changes.")
                                             : tr("Daemon unavailable. No hardware changes.")));
    } else {
        text_.setText(refresh_.status == application::display::Status::Ready
                          ? tr("Display refresh rate: %1 Hz").arg(refresh_.observed_hz)
                          : tr("Display refresh change could not be confirmed"));
    }
    update_icon();
}
void OsdPresenter::reveal() {
    preferences_.refresh();
    if (preferences_.osd_sound())
        QApplication::beep();
    if (!preferences_.osd_enabled()) {
        timeout_.stop();
        hide();
        return;
    }
    retranslate();
    adjustSize();
    if (const auto *screen = resolve_display(preferences_.osd_screen(), QCursor::pos()))
        move(feedback_position(screen->availableGeometry(), size(),
                               preferences_.osd_position().toStdString()));
    show();
    raise();
    timeout_.start(2200);
}

application::feedback::Delivery OsdPresenter::present(application::feedback::Event event) {
    using namespace application::feedback;
    if (!valid(event))
        return Delivery::Invalid;
    if (!distinct_.accept(event))
        return Delivery::Suppressed;
    switch (event.kind) {
    case application::feedback::Kind::Microphone:
        kind_ = Kind::Microphone;
        microphone_ = {application::microphone::Status::Ready, event.value != 0, {}};
        break;
    case application::feedback::Kind::Performance:
        kind_ = Kind::Profile;
        profile_ = static_cast<std::uint8_t>(event.value);
        break;
    case application::feedback::Kind::Refresh:
        kind_ = Kind::Refresh;
        refresh_ = {application::display::Status::Ready, event.value, {}};
        break;
    }
    reveal();
    return Delivery::Presented;
}
void OsdPresenter::present(const application::microphone::Observation &observation) {
    if (observation.status == application::microphone::Status::Ready && observation.muted) {
        (void)present(application::feedback::Event{application::feedback::Kind::Microphone,
                                                   *observation.muted ? 1U : 0U});
        return;
    }
    distinct_.forget(application::feedback::Kind::Microphone);
    microphone_ = observation;
    kind_ = Kind::Microphone;
    reveal();
}
void OsdPresenter::show_result(const application::DiagnosticReply &reply) {
    diagnostic_ = reply;
    kind_ = Kind::Diagnostic;
    reveal();
}
void OsdPresenter::update_icon() {
    QString name;
    switch (kind_) {
    case Kind::Mi:
        name = QStringLiteral("manager");
        break;
    case Kind::Profile:
        name = profile_ == 1   ? QStringLiteral("balanced")
               : profile_ == 2 ? QStringLiteral("quiet")
                               : QStringLiteral("turbo");
        break;
    case Kind::Refresh:
        name = refresh_.status == application::display::Status::Ready ? QStringLiteral("display")
                                                                      : QStringLiteral("warning");
        break;
    case Kind::Microphone:
        name = microphone_.status != application::microphone::Status::Ready || !microphone_.muted
                   ? QStringLiteral("warning")
               : *microphone_.muted ? QStringLiteral("microphone-muted")
                                    : QStringLiteral("microphone-active");
        break;
    case Kind::Diagnostic:
        name = QStringLiteral("diagnostic");
        break;
    }
    const int extent = text_.fontMetrics().height();
    icon_.setFixedSize(extent, extent);
    icon_.setPixmap(symbol_pixmap(name, palette().color(QPalette::WindowText), extent, devicePixelRatioF()));
}
} // namespace manager::presentation
