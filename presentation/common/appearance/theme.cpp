#include "presentation/common/appearance/theme.hpp"
#include <QApplication>
#include <QEvent>
#include <QStyle>
#include <QStyleHints>
#include <QTimer>

#include <QAccessibilityHints>
namespace manager::presentation {
namespace {
void dark_palette(QPalette &palette) {
    palette.setColor(QPalette::Window, QColor(QStringLiteral("#25282d")));
    palette.setColor(QPalette::Base, QColor(QStringLiteral("#191c20")));
    palette.setColor(QPalette::Button, QColor(QStringLiteral("#30343b")));
    palette.setColor(QPalette::AlternateBase, QColor(QStringLiteral("#30343b")));
    palette.setColor(QPalette::ToolTipBase, QColor(QStringLiteral("#30343b")));
    for (auto role : {QPalette::WindowText, QPalette::Text, QPalette::ButtonText, QPalette::ToolTipText}) {
        palette.setColor(role, QColor(QStringLiteral("#e7eaf0")));
        palette.setColor(QPalette::Disabled, role, QColor(QStringLiteral("#9da5b2")));
    }
    palette.setColor(QPalette::PlaceholderText, QColor(QStringLiteral("#9da5b2")));
    palette.setColor(QPalette::Mid, QColor(QStringLiteral("#646a74")));
}

void neutral_palette(QPalette &palette) {
    const auto window = palette.color(QPalette::Window);
    const bool dark = window.lightness() < 128;
    const auto surface = dark ? window.lighter(145) : window.darker(104);
    palette.setColor(QPalette::AlternateBase, surface);
    palette.setColor(QPalette::Accent, QColor(dark ? QStringLiteral("#dedede") : QStringLiteral("#555555")));
    palette.setColor(QPalette::Highlight, palette.color(QPalette::Accent));
    palette.setColor(QPalette::HighlightedText,
                     QColor(dark ? QStringLiteral("#202020") : QStringLiteral("#ffffff")));
}
} // namespace

ThemeController::ThemeController() : system_palette_(QGuiApplication::palette()) {
    qApp->installEventFilter(this);
    connect(QGuiApplication::styleHints()->accessibility(), &QAccessibilityHints::contrastPreferenceChanged,
            this, [this] { schedule_palette(); });
    connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged, this,
            [this] { schedule_palette(); });
}
void ThemeController::schedule_palette() {
    if (applying_ || pending_) {
        return;
    }
    pending_ = true;
    QTimer::singleShot(0, this, [this] {
        pending_ = false;
        apply_palette();
    });
}

bool ThemeController::eventFilter(QObject *watched, QEvent *event) {
    if (watched == qApp && event->type() == QEvent::ApplicationPaletteChange && !applying_) {
        if (mode_ == QStringLiteral("system")) {
            system_palette_ = QGuiApplication::palette();
        }
        emit changed();
    }
    return QObject::eventFilter(watched, event);
}

void ThemeController::set_mode(const QString &mode) {
    mode_ = mode;
    auto *hints = QGuiApplication::styleHints();
    if (mode == QStringLiteral("system")) {
        hints->unsetColorScheme();
    } else {
        hints->setColorScheme(mode == QStringLiteral("dark") ? Qt::ColorScheme::Dark
                                                             : Qt::ColorScheme::Light);
    }
    apply_palette();
}

void ThemeController::apply_palette() {
    applying_ = true;
    const bool highContrast = QGuiApplication::styleHints()->accessibility()->contrastPreference() ==
                              Qt::ContrastPreference::HighContrast;
    if (mode_ == QStringLiteral("system") || highContrast) {
        QApplication::setPalette(QPalette{});
        QGuiApplication::setPalette(QPalette{});
        system_palette_ = QGuiApplication::palette();
    }
    QPalette palette = (mode_ == QStringLiteral("system") || highContrast)
                           ? system_palette_
                           : QApplication::style()->standardPalette();
    if (!highContrast) {
        if (mode_ == QStringLiteral("dark")) {
            dark_palette(palette);
        }
        neutral_palette(palette);
    }
    QGuiApplication::setPalette(palette);
    QApplication::setPalette(palette);
    applying_ = false;
    emit changed();
}
} // namespace manager::presentation
