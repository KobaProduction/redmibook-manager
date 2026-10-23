#include "apps/desktop/style_setup.hpp"
#include "adapters/local_api/session_client.hpp"
#include <QGuiApplication>
#include <QQuickStyle>

namespace manager::desktop {
void configure_controls(const QString &) {
    if (QGuiApplication::platformName() == QStringLiteral("offscreen")) {
        // Preview the requested public Qt style without attaching to the desktop.
        const auto requested = qEnvironmentVariable("QT_QUICK_CONTROLS_STYLE");
        QQuickStyle::setStyle(requested == QStringLiteral("FluentWinUI3") ? requested
                                                                          : QStringLiteral("Fusion"));
        return;
    }
    QQuickStyle::setStyle(QStringLiteral("FluentWinUI3"));
}
} // namespace manager::desktop
