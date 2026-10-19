#include "presentation/osd/diagnostic_osd.hpp"
#include "presentation/common/displays.hpp"
#include "presentation/osd/placement.hpp"
#include <QCursor>
#include <QEvent>
#include <QGuiApplication>
#include <QScreen>
#include <QVBoxLayout>

namespace manager::presentation {
DiagnosticOsd::DiagnosticOsd(Preferences &preferences)
    : RoundedFrame(Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint), preferences_(preferences) {
    setAttribute(Qt::WA_ShowWithoutActivating);
    set_rounded(preferences_.rounded_corners());
    connect(&preferences_, &Preferences::changed, this,
            [this] { set_rounded(preferences_.rounded_corners()); });
    setFrameStyle(QFrame::StyledPanel | QFrame::Raised);
    // Receive native session shutdown requests before the first popup is shown.
    static_cast<void>(winId());
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 18, 24, 18);
    layout->addWidget(&title_);
    layout->addWidget(&detail_);
    hide_.setSingleShot(true);
    connect(&hide_, &QTimer::timeout, this, &QWidget::hide);
}

void DiagnosticOsd::retranslate() {
    title_.setText(tr("Diagnostic signal received"));
    detail_.setText(reply_.connected ? tr("Daemon responded. Test only — no hardware changes.")
                                     : tr("Daemon unavailable. No hardware changes."));
}

void DiagnosticOsd::relocate() {
    if (const auto *screen = resolve_display(preferences_.osd_screen(), QCursor::pos())) {
        move(feedback_position(screen->availableGeometry(), size(),
                               preferences_.osd_position().toStdString()));
    }
}

void DiagnosticOsd::show_result(const application::DiagnosticReply &reply) {
    preferences_.refresh();
    reply_ = reply;
    retranslate();
    adjustSize();
    relocate();
    show();
    hide_.start(2200);
}

void DiagnosticOsd::changeEvent(QEvent *event) {
    if (event->type() == QEvent::LanguageChange) {
        retranslate();
    }
    QFrame::changeEvent(event);
}
} // namespace manager::presentation
