#include "presentation/updates/update_notice.hpp"
#include "presentation/common/displays.hpp"
#include "presentation/osd/placement.hpp"
#include <QCursor>
#include <QVBoxLayout>
namespace manager::presentation {
UpdateNotice::UpdateNotice(Preferences &preferences)
    : RoundedFrame(Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint), preferences_(preferences) {
    setAttribute(Qt::WA_ShowWithoutActivating);
    set_rounded(preferences_.rounded_corners());
    connect(&preferences_, &Preferences::changed, this,
            [this] { set_rounded(preferences_.rounded_corners()); });
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(18, 18, 18, 18);
    layout->addWidget(&title_);
    layout->addWidget(&download_);
    layout->addWidget(&dismiss_);
    connect(&download_, &QPushButton::clicked, this, [this] {
        emit downloadRequested(url_);
        hide();
    });
    connect(&dismiss_, &QPushButton::clicked, this, &QWidget::hide);
    connect(&preferences_, &Preferences::languageChanged, this, &UpdateNotice::retranslate);
    hide_.setSingleShot(true);
    connect(&hide_, &QTimer::timeout, this, &QWidget::hide);
}
void UpdateNotice::show_result(const application::UpdateResult &result) {
    if (result.state != application::UpdateState::Available) {
        return;
    }
    version_ = QString::fromStdString(result.version);
    retranslate();
    url_ = QString::fromStdString(result.download_url.empty() ? result.release_url : result.download_url);
    adjustSize();
    if (const auto *screen = resolve_display(preferences_.osd_screen(), QCursor::pos())) {
        move(feedback_position(screen->availableGeometry(), size(),
                               preferences_.osd_position().toStdString()));
    }
    show();
    hide_.start(15000);
}

void UpdateNotice::retranslate() {
    title_.setText(tr("Update available: %1").arg(version_));
    download_.setText(tr("Download update"));
    dismiss_.setText(tr("Later"));
    adjustSize();
}
} // namespace manager::presentation
