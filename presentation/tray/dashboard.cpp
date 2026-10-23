#include "presentation/tray/dashboard.hpp"
#include "presentation/common/icons.hpp"
#include <QCursor>
#include <QEvent>
#include <QFontMetrics>
#include <QGuiApplication>
#include <QIcon>
#include <QLocale>
#include <QPointer>
#include <QScreen>
#include <QStyle>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>
#include <utility>

namespace manager::presentation {
bool TrayDashboard::battery_only_change(const TrayDeviceState &after) const {
    auto controls_before = device_state_;
    controls_before.chargeLimit = after.chargeLimit;
    controls_before.batteryRateW = after.batteryRateW;
    controls_before.batteryDischarging = after.batteryDischarging;
    return controls_before == after;
}
TrayDashboard::TrayDashboard(RuntimeMonitor &runtime)
    : RoundedFrame(Qt::Popup | Qt::FramelessWindowHint), runtime_(runtime) {
    setObjectName(QStringLiteral("trayDashboard"));
    setFrameStyle(QFrame::StyledPanel);
    setAutoFillBackground(false);
    static_cast<void>(winId());
    setFixedWidth(400);
    configure_header();
    build_layout();
    connect_actions();
    retranslate();
}

void TrayDashboard::setPreferenceConnection(bool available, const QString &text) {
    preferences_available_ = available;
    connection_notice_.setText(text);
    connection_notice_.setVisible(!available);
    reconnect_.setText(tr("Retry connection"));
    reconnect_.setVisible(!available);
    performance_section_.setEnabled(available);
    refresh_section_.setEnabled(available);
    presets_.setEnabled(available && !device_state_.pending);
    update_runtime_icon();
    if (isVisible())
        place_dashboard();
}
void TrayDashboard::configure_header() {
    title_.setObjectName(QStringLiteral("dashboardTitle"));
    version_.setObjectName(QStringLiteral("dashboardVersion"));
    status_.setObjectName(QStringLiteral("dashboardRuntimeIcon"));
    microphone_icon_.setObjectName(QStringLiteral("dashboardMicrophoneIcon"));
    auto heading = title_.font();
    heading.setPointSize(12);
    heading.setBold(true);
    title_.setFont(heading);
    version_.setForegroundRole(QPalette::PlaceholderText);
    status_.setAlignment(Qt::AlignCenter);
    microphone_icon_.setAlignment(Qt::AlignCenter);
}

void TrayDashboard::showEvent(QShowEvent *event) {
    RoundedFrame::showEvent(event);
    place_dashboard();
    emit visibilityChanged(true);
}

void TrayDashboard::hideEvent(QHideEvent *event) {
    RoundedFrame::hideEvent(event);
    emit visibilityChanged(false);
}

QVBoxLayout *TrayDashboard::build_header() {
    auto *brand = new QVBoxLayout;
    auto *heading = new QHBoxLayout;
    heading->setSpacing(6);
    heading->addWidget(&title_, 0, Qt::AlignVCenter);
    heading->addWidget(&version_, 0, Qt::AlignVCenter);
    heading->addStretch();
    heading->addWidget(&battery_power_, 0, Qt::AlignVCenter);
    auto *battery = new QHBoxLayout(&battery_box_);
    battery->setContentsMargins(0, 0, 0, 0);
    battery_icon_.setAlignment(Qt::AlignCenter);
    battery->addWidget(&battery_icon_);
    heading->addWidget(&battery_box_, 0, Qt::AlignVCenter);
    heading->addWidget(&microphone_icon_, 0, Qt::AlignVCenter);
    heading->addWidget(&status_, 0, Qt::AlignVCenter);
    brand->addLayout(heading);
    return brand;
}

void TrayDashboard::build_layout() {
    auto *layout = new QVBoxLayout(this);
    // Popup geometry is bounded by the screen, not a retained window minimum.
    layout->setSizeConstraint(QLayout::SetNoConstraint);
    layout->setContentsMargins(16, 14, 16, 14);
    layout->setSpacing(10);
    layout->addLayout(build_header());
    connection_notice_.setWordWrap(true);
    connection_notice_.hide();
    layout->addWidget(&connection_notice_);
    reconnect_.hide();
    layout->addWidget(&reconnect_);
    connect(&reconnect_, &QPushButton::clicked, this, &TrayDashboard::reconnectRequested);
    build_body(*layout);
    configurable_sections_ = {{"performance", &performance_section_},
                              {"refresh", &refresh_section_},
                              {"microphone", &microphone_icon_},
                              {"connection", &status_},
                              {"battery", &battery_box_},
                              {"batteryPower", &battery_power_}};
    layout->addWidget(&open_);
}

void TrayDashboard::build_body(QVBoxLayout &layout) {
    auto *performance = new QVBoxLayout(&performance_section_);
    performance->setContentsMargins(0, 0, 0, 0);
    performance->addWidget(&mode_label_);
    build_modes(*performance);
    layout.addWidget(&performance_section_);
    auto *refresh = new QVBoxLayout(&refresh_section_);
    refresh->setContentsMargins(0, 0, 0, 0);
    refresh->addWidget(&refresh_label_);
    build_refresh(*refresh);
    layout.addWidget(&refresh_section_);
    preset_scroll_.setObjectName(QStringLiteral("presetScroll"));
    preset_scroll_.setWidgetResizable(true);
    preset_scroll_.setFrameShape(QFrame::NoFrame);
    preset_scroll_.setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    preset_scroll_.setWidget(&presets_);
    preset_scroll_.hide();
    layout.addWidget(&preset_scroll_);
    connect(&presets_, &PresetPanel::requested, this, &TrayDashboard::presetRequested);
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
}

void TrayDashboard::retranslate() {
    title_.setText(tr("RedmiBook Manager"));
    version_.setText(QStringLiteral(MANAGER_APP_VERSION));
    const auto &core = runtime_.reply().daemon;
    version_.setToolTip(tr("Core: %1 (%2)\nTray: %3 (%4)")
                            .arg(QString::fromStdString(core.version), QString::fromStdString(core.build),
                                 QStringLiteral(MANAGER_APP_VERSION), QStringLiteral(MANAGER_BUILD_ID)));
    update_runtime_icon();
    mode_label_.setText(tr("Performance mode"));
    refresh_label_.setText(tr("Screen refresh rate"));
    modes_[0].setText(tr("Quiet"));
    modes_[1].setText(tr("Balanced"));
    modes_[2].setText(tr("Performance"));
    open_.setText(tr("Open Manager"));
    update_device_controls();
}

void TrayDashboard::update_presets() {
    presets_.updatePresets(device_state_.presets, device_state_.pending);
    preset_scroll_.setVisible(!presets_.isHidden());
    presets_.setEnabled(preferences_available_ && !device_state_.pending);
}

void TrayDashboard::setDeviceState(TrayDeviceState state) {
    if (state == device_state_)
        return;
    const bool battery_only = battery_only_change(state);
    const bool new_error = state.error && (!device_state_.error || state.status != device_state_.status);
    device_state_ = std::move(state);
    update_presets();
    if (battery_only) {
        update_battery(false);
        update_sections();
    } else {
        update_device_controls();
        update_runtime_icon();
        if (isVisible())
            place_dashboard();
    }
    if (new_error)
        emit errorOccurred(device_state_.status);
}

void TrayDashboard::build_refresh(QVBoxLayout &layout) {
    auto *row = new QHBoxLayout;
    row->setSpacing(4);
    for (std::size_t index = 0; index < refresh_buttons_.size(); ++index) {
        auto &button = refresh_buttons_[index];
        button.setCheckable(true);
        button.setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        connect(&button, &QToolButton::clicked, this, [this, index] {
            update_refresh();
            emit refreshRequested(refresh_choices[index]);
        });
        row->addWidget(&button);
    }
    layout.addLayout(row);
}
void TrayDashboard::update_refresh() {
    for (std::size_t index = 0; index < refresh_buttons_.size(); ++index) {
        auto &button = refresh_buttons_[index];
        const auto hz = refresh_choices[index];
        button.setText(tr("%1 Hz").arg(hz));
        button.setChecked(device_state_.refreshHz == static_cast<int>(hz));
        button.setVisible(std::ranges::find(device_state_.refreshRates, hz) !=
                          device_state_.refreshRates.end());
        button.setEnabled(device_state_.refreshAvailable && !device_state_.pending);
    }
}

void TrayDashboard::update_device_controls() {
    mode_group_.setExclusive(false);
    for (std::size_t index = 0; index < modes_.size(); ++index) {
        modes_[index].setChecked(static_cast<int>(index) == device_state_.profileIndex);
        modes_[index].setEnabled(device_state_.profileAvailable && !device_state_.pending);
    }
    mode_group_.setExclusive(true);
    microphone_icon_.setToolTip(microphone_text());
    microphone_icon_.setAccessibleName(microphone_text());
    update_microphone_icon();
    update_refresh();
    update_battery();
    update_sections();
}

void TrayDashboard::update_microphone_icon() {
    microphone_icon_.setVisible(device_state_.microphoneAvailable && section_enabled("microphone"));
    const auto resource = device_state_.microphoneMuted ? QStringLiteral("microphone-muted")
                                                        : QStringLiteral("microphone-active");
    const int extent = 20;
    microphone_icon_.setFixedSize(22, 22);
    microphone_icon_.setPixmap(
        symbol_pixmap(resource, palette().color(QPalette::WindowText), extent, devicePixelRatioF()));
}

void TrayDashboard::update_runtime_icon() {
    const auto health =
        application::connection_health(runtime_.reply(), {MANAGER_APP_VERSION, MANAGER_BUILD_ID});
    const auto symbol = health == application::ConnectionHealth::Ready && preferences_available_
                            ? QStringLiteral("service-connected")
                        : health == application::ConnectionHealth::VersionMismatch
                            ? QStringLiteral("warning")
                            : QStringLiteral("service-disconnected");
    const int extent = 20;
    status_.setFixedSize(22, 22);
    status_.setPixmap(
        symbol_pixmap(symbol, palette().color(QPalette::WindowText), extent, devicePixelRatioF()));
    const auto description = (preferences_available_ ? runtime_.status_text() : connection_notice_.text()) +
                             (device_state_.status.isEmpty() ? QString{} : '\n' + device_state_.status);
    status_.setToolTip(description);
    status_.setAccessibleName(description);
}

QString TrayDashboard::microphone_text() const {
    if (!device_state_.microphoneAvailable) {
        return tr("Microphone unavailable");
    }
    return device_state_.microphoneMuted ? tr("Microphone: muted") : tr("Microphone: on");
}
bool TrayDashboard::section_enabled(std::string_view id) const {
    return std::find(device_state_.sections.begin(), device_state_.sections.end(), id) !=
           device_state_.sections.end();
}
void TrayDashboard::update_sections() {
    for (const auto &[id, widget] : configurable_sections_)
        widget->setVisible(section_enabled(id));
    performance_section_.setVisible(section_enabled("performance") &&
                                    (device_state_.profileSupported || device_state_.profileAvailable));
    refresh_section_.setVisible(section_enabled("refresh") &&
                                (device_state_.refreshSupported || device_state_.refreshAvailable));
    microphone_icon_.setVisible(section_enabled("microphone") && device_state_.microphoneAvailable);
    battery_box_.setVisible(section_enabled("battery") && device_state_.chargeLimit > 0);
    battery_power_.setVisible(section_enabled("batteryPower") && device_state_.batteryRateW.has_value() &&
                              std::abs(*device_state_.batteryRateW) >= 0.05);
}
void TrayDashboard::update_battery(bool redrawIcon) {
    const int extent = 20;
    battery_icon_.setFixedSize(22, 22);
    if (redrawIcon)
        battery_icon_.setPixmap(symbol_pixmap(QStringLiteral("battery-limit"),
                                              palette().color(QPalette::WindowText), extent,
                                              devicePixelRatioF()));
    battery_power_.setFont(version_.font());
    const auto description = tr("Charge limit: %1%").arg(device_state_.chargeLimit);
    battery_box_.setToolTip(description);
    battery_icon_.setAccessibleName(description);
    const auto rate = device_state_.batteryRateW.value_or(0);
    battery_power_.setText((rate < 0 ? QStringLiteral("↓ ") : QStringLiteral("↑ ")) +
                           tr("%1 W").arg(QLocale().toString(std::abs(rate), 'f', 1)));
    battery_power_.setToolTip(rate < 0 ? tr("Discharge") : tr("Charge"));
}

void TrayDashboard::toggle(const QRect &anchor) {
    if (isVisible()) {
        hide();
        return;
    }
    anchor_ = anchor.isEmpty() ? QRect(QCursor::pos(), QSize(1, 1)) : anchor;
    place_dashboard();
    show();
    runtime_.refresh();
}

void TrayDashboard::fit_to_screen(const QRect &area) {
    auto *box = layout();
    box->activate();
    const int required = std::max({box->totalMinimumSize().height(), box->totalSizeHint().height(),
                                   box->totalHeightForWidth(width()), box->minimumHeightForWidth(width())});
    const int reserved = std::max(0, required - preset_scroll_.minimumHeight());
    const int budget = std::max(0, area.height() - reserved - 16);
    preset_scroll_.setFixedHeight(std::min(presets_.sizeHint().height(), budget));
    box->invalidate();
    box->activate();
    adjustSize();
}

void TrayDashboard::place_dashboard() {
    const auto point = anchor_.center();
    auto *screen = QGuiApplication::screenAt(point);
    if (screen == nullptr) {
        screen = QGuiApplication::primaryScreen();
    }
    if (screen == nullptr) {
        return;
    }
    const auto area = screen->availableGeometry();
    fit_to_screen(area);
    const int x =
        std::clamp(point.x() - width() / 2, area.left(), std::max(area.left(), area.right() - width()));
    const int y = point.y() > area.center().y() ? point.y() - height() - 12 : point.y() + 12;
    move(x, std::clamp(y, area.top(), std::max(area.top(), area.bottom() - height())));
}

void TrayDashboard::changeEvent(QEvent *event) {
    if (event->type() == QEvent::LanguageChange || event->type() == QEvent::PaletteChange ||
        event->type() == QEvent::FontChange) {
        retranslate();
    }
    QFrame::changeEvent(event);
}
} // namespace manager::presentation
