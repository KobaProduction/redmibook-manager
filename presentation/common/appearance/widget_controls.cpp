#include "presentation/common/appearance/widget_controls.hpp"

namespace manager::presentation {
QString surface_controls_style(bool rounded) {
    return QStringLiteral(
               "QPushButton, QToolButton { background: palette(button); color: palette(button-text); "
               "border: 1px solid palette(mid); border-radius: %1px; }"
               "QPushButton { padding: 6px 12px; min-height: 20px; }"
               "QToolButton { padding: 5px 3px; }"
               "QPushButton:hover, QToolButton:hover { background: palette(alternate-base); }"
               "QPushButton:pressed, QToolButton:pressed { background: palette(mid); }"
               "QPushButton:focus, QToolButton:focus { border-color: palette(highlight); }"
               "QToolButton:checked { background: palette(highlight); color: palette(highlighted-text); }"
               "QPushButton:disabled, QToolButton:disabled { color: palette(button-text); }")
        .arg(rounded ? 4 : 0);
}

QString popup_menu_style(bool rounded) {
    return QStringLiteral(
               "QMenu { background: palette(window); color: palette(window-text); "
               "border: 1px solid palette(mid); border-radius: %1px; padding: 6px; }"
               "QMenu::item { padding: 6px 12px; border-radius: %2px; }"
               "QMenu::item:selected { background: palette(highlight); color: palette(highlighted-text); }"
               "QMenu::separator { height: 1px; background: palette(mid); margin: 4px; }")
        .arg(rounded ? 8 : 0)
        .arg(rounded ? 4 : 0);
}
} // namespace manager::presentation
