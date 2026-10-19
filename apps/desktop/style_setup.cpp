#include "apps/desktop/style_setup.hpp"
#include "adapters/local_api/session_client.hpp"
#include "adapters/storage/ui_preferences/store.hpp"
#include <QGuiApplication>
#include <QQuickStyle>

namespace manager::desktop {
void configure_controls() {
    if (QGuiApplication::platformName() == QStringLiteral("offscreen")) {
        // Fluent image assets require a native color-scheme backend.
        QQuickStyle::setStyle(QStringLiteral("Fusion"));
        return;
    }
    adapters::UiPreferenceStore store(adapters::preferences_directory());
    const auto saved = store.read();
    QQuickStyle::setStyle(saved && saved->design == "classic" ? QStringLiteral("Fusion")
                                                              : QStringLiteral("FluentWinUI3"));
}
} // namespace manager::desktop
