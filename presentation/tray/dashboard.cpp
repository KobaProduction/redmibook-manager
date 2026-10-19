#include "presentation/tray/dashboard.hpp"
#include <QCursor>
#include <QEvent>
#include <QGuiApplication>
#include <QIcon>
#include <QPointer>
#include <QScreen>
#include <QVBoxLayout>
#include <algorithm>
#include <utility>

namespace manager::presentation {
TrayDashboard::TrayDashboard(RuntimeMonitor &runtime)
    : RoundedFrame(Qt::Popup | Qt::FramelessWindowHint), runtime_(runtime) {
    setObjectName(QStringLiteral("trayDashboard"));
    setFrameStyle(QFrame::StyledPanel);
    setAutoFillBackground(false);
    static_cast<void>(winId());
    setFixedWidth(360);
    device_status_.setWordWrap(true);
    configure_header();
    build_layout();
    connect_actions();
    retranslate();
}

void TrayDashboard::configure_header() {
    title_.setObjectName(QStringLiteral("dashboardTitle"));
    version_.setObjectName(QStringLiteral("dashboardVersion"));
    auto heading = title_.font();
    heading.setPointSize(12);
    heading.setBold(true);
    title_.setFont(heading);
    auto small = publisher_.font();
    small.setPointSize(8);
    small.setItalic(true);
    small.setLetterSpacing(QFont::AbsoluteSpacing, 0.7);
    publisher_.setFont(small);
    version_.setForegroundRole(QPalette::PlaceholderText);
}

QVBoxLayout *TrayDashboard::build_header() {
    auto *brand = new QVBoxLayout;
    brand->setSpacing(2);
    auto *heading = new QHBoxLayout;
    heading->setSpacing(8);
    heading->addWidget(&title_);
    heading->addWidget(&version_, 0, Qt::AlignVCenter);
    heading->addStretch();
    brand->addLayout(heading);
    brand->addWidget(&publisher_);
    return brand;
}

void TrayDashboard::build_layout() {
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 14, 16, 14);
    layout->setSpacing(10);
    layout->addLayout(build_header());
    layout->addSpacing(4);
    layout->addWidget(&status_);
    layout->addWidget(&mode_label_);
    build_modes(*layout);
    auto *devices = new QHBoxLayout;
    devices->addWidget(&microphone_icon_);
    devices->addWidget(&microphone_, 1);
    devices->addWidget(&refresh_);
    layout->addLayout(devices);
    layout->addWidget(&device_status_);
    layout->addWidget(&open_);
}

void TrayDashboard::build_modes(QVBoxLayout &layout) {
    auto *row = new QHBoxLayout;
    row->setSpacing(4);
    for (std::size_t index = 0; index < modes_.size(); ++index) {
        auto &button = modes_[index];
        button.setObjectName(QStringLiteral("performanceMode%1").arg(index));
        button.setCheckable(true);
        button.setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        mode_group_.addButton(&button, static_cast<int>(index));
        row->addWidget(&button);
    }
    layout.addLayout(row);
}

void TrayDashboard::connect_actions() {
    connect(&runtime_, &RuntimeMonitor::changed, this, &TrayDashboard::retranslate);
    connect(&open_, &QPushButton::clicked, this, [this] {
        hide();
        emit openManager();
    });
    connect(&mode_group_, &QButtonGroup::idClicked, this, [this](int index) {
        update_device_controls();
        emit profileRequested(index);
    });
    connect(&refresh_, &QToolButton::clicked, this, &TrayDashboard::refreshRequested);
}

void TrayDashboard::retranslate() {
    title_.setText(tr("RedmiBook Manager"));
    publisher_.setText(QStringLiteral("Koba Production"));
    version_.setText(QStringLiteral(MANAGER_APP_VERSION));
    const auto &core = runtime_.reply().daemon;
    version_.setToolTip(tr("Core: %1 (%2)\nTray: %3 (%4)")
                            .arg(QString::fromStdString(core.version), QString::fromStdString(core.build),
                                 QStringLiteral(MANAGER_APP_VERSION), QStringLiteral(MANAGER_BUILD_ID)));
    status_.setText(runtime_.status_text());
    status_.setWordWrap(true);
    mode_label_.setText(tr("Performance mode"));
    modes_[0].setText(tr("Balanced"));
    modes_[1].setText(tr("Quiet"));
    modes_[2].setText(tr("Turbo"));
    open_.setText(tr("Open Manager"));
    update_device_controls();
}

void TrayDashboard::setDeviceState(TrayDeviceState state) {
    device_state_ = std::move(state);
    update_device_controls();
}

void TrayDashboard::update_device_controls() {
    mode_group_.setExclusive(false);
    for (std::size_t index = 0; index < modes_.size(); ++index) {
        modes_[index].setChecked(static_cast<int>(index) == device_state_.profileIndex);
        modes_[index].setEnabled(device_state_.profileAvailable && !device_state_.pending);
    }
    mode_group_.setExclusive(true);
    microphone_.setText(microphone_text());
    microphone_.setAccessibleName(microphone_.text());
    update_microphone_icon();
    refresh_.setText(device_state_.refreshHz > 0 ? tr("%1 Hz").arg(device_state_.refreshHz)
                                                 : tr("Display unavailable"));
    refresh_.setToolTip(tr("Switch display refresh rate"));
    refresh_.setEnabled(device_state_.refreshAvailable && !device_state_.pending);
    device_status_.setText(device_state_.status);
    device_status_.setVisible(!device_state_.status.isEmpty());
}

void TrayDashboard::update_microphone_icon() {
    microphone_icon_.setVisible(device_state_.microphoneAvailable);
    const auto resource = device_state_.microphoneMuted ? QStringLiteral(":/icons/microphone-muted.svg")
                                                        : QStringLiteral(":/icons/microphone-active.svg");
    microphone_icon_.setPixmap(QIcon(resource).pixmap(20, 20));
}

QString TrayDashboard::microphone_text() const {
    if (!device_state_.microphoneAvailable) {
        return tr("Microphone unavailable");
    }
    return device_state_.microphoneMuted ? tr("Microphone: muted") : tr("Microphone: on");
}

void TrayDashboard::toggle(const QRect &anchor) {
    if (isVisible()) {
        hide();
        return;
    }
    adjustSize();
    const auto point = anchor.isEmpty() ? QCursor::pos() : anchor.center();
    auto *screen = QGuiApplication::screenAt(point);
    if (screen == nullptr) {
        screen = QGuiApplication::primaryScreen();
    }
    if (screen == nullptr) {
        return;
    }
    const auto area = screen->availableGeometry();
    const int x =
        std::clamp(point.x() - width() / 2, area.left(), std::max(area.left(), area.right() - width()));
    const int y = point.y() > area.center().y() ? point.y() - height() - 12 : point.y() + 12;
    move(x, std::clamp(y, area.top(), std::max(area.top(), area.bottom() - height())));
    show();
    runtime_.refresh();
}

void TrayDashboard::changeEvent(QEvent *event) {
    if (event->type() == QEvent::LanguageChange) {
        retranslate();
    }
    QFrame::changeEvent(event);
}
} // namespace manager::presentation
