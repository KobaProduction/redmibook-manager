#include "presentation/osd/microphone_osd.hpp"
#include "presentation/common/displays.hpp"
#include "presentation/osd/placement.hpp"
#include <QCursor>
#include <QHBoxLayout>
#include <QIcon>

namespace manager::presentation {
MicrophoneOsd::MicrophoneOsd(Preferences &preferences)
    : RoundedFrame(Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint), preferences_(preferences) {
    setAttribute(Qt::WA_ShowWithoutActivating);
    set_rounded(preferences_.rounded_corners());
    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(24, 18, 24, 18);
    layout->setSpacing(12);
    icon_.setFixedSize(32, 32);
    layout->addWidget(&icon_);
    layout->addWidget(&text_);
    timeout_.setSingleShot(true);
    connect(&timeout_, &QTimer::timeout, this, &QWidget::hide);
    connect(&preferences_, &Preferences::languageChanged, this, &MicrophoneOsd::retranslate);
    connect(&preferences_, &Preferences::changed, this,
            [this] { set_rounded(preferences_.rounded_corners()); });
}
void MicrophoneOsd::retranslate() {
    const bool known =
        observation_.status == application::microphone::Status::Ready && observation_.muted.has_value();
    text_.setText(known ? (*observation_.muted ? tr("Microphone muted") : tr("Microphone active"))
                        : tr("Microphone change could not be confirmed"));
    icon_.setVisible(known);
    if (known) {
        const auto path = *observation_.muted ? QStringLiteral(":/icons/microphone-muted.svg")
                                              : QStringLiteral(":/icons/microphone-active.svg");
        icon_.setPixmap(QIcon(path).pixmap(QSize(32, 32), devicePixelRatioF()));
    }
}
void MicrophoneOsd::present(const application::microphone::Observation &observation) {
    preferences_.refresh();
    observation_ = observation;
    retranslate();
    adjustSize();
    if (const auto *screen = resolve_display(preferences_.osd_screen(), QCursor::pos())) {
        move(feedback_position(screen->availableGeometry(), size(),
                               preferences_.osd_position().toStdString()));
    }
    show();
    raise();
    timeout_.start(2200);
}
} // namespace manager::presentation
