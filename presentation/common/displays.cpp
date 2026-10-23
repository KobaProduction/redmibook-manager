#include "presentation/common/displays.hpp"
#include <QCryptographicHash>
#include <QDebug>
#include <QEvent>
#include <QGuiApplication>
#include <QTimer>
#include <algorithm>
#include <utility>

namespace manager::presentation {
namespace {
class WindowActivation final : public QObject {
  public:
    explicit WindowActivation(QWindow &window) : QObject(&window), window_(window) {
        window_.installEventFilter(this);
        deadline_.setSingleShot(true);
        connect(&deadline_, &QTimer::timeout, this, [this] {
            pending_ = false;
            native_focus_ = {};
            qWarning("Window activation expired before exposure");
        });
    }
    void request(std::function<void()> native_focus) {
        native_focus_ = std::move(native_focus);
        pending_ = true;
        deadline_.start(5000);
        QTimer::singleShot(0, this, [this] { activate(); });
    }

  protected:
    bool eventFilter(QObject *object, QEvent *event) override {
        if (event->type() == QEvent::Expose)
            activate();
        return QObject::eventFilter(object, event);
    }

  private:
    QWindow &window_;
    QTimer deadline_;
    std::function<void()> native_focus_;
    bool pending_{false};
    void activate() {
        if (!pending_ || !window_.isExposed())
            return;
        pending_ = false;
        deadline_.stop();
        window_.raise();
        window_.requestActivate();
        const auto focus = std::exchange(native_focus_, {});
        if (focus)
            focus();
    }
};
void request_activation(QWindow &window, std::function<void()> native_focus) {
    for (auto *child : window.children()) {
        if (auto *activation = dynamic_cast<WindowActivation *>(child)) {
            activation->request(std::move(native_focus));
            return;
        }
    }
    auto *activation = new WindowActivation(window);
    activation->request(std::move(native_focus));
}
} // namespace
QString display_id(const QScreen &screen) {
    const auto identity = screen.name() + screen.manufacturer() + screen.model() + screen.serialNumber();
    return QString::fromLatin1(
        QCryptographicHash::hash(identity.toUtf8(), QCryptographicHash::Sha256).toHex());
}
QScreen *resolve_display(const QString &id, const QPoint &anchor) {
    for (auto *screen : QGuiApplication::screens()) {
        if (display_id(*screen) == id) {
            return screen;
        }
    }
    auto *active = QGuiApplication::screenAt(anchor);
    return active == nullptr ? QGuiApplication::primaryScreen() : active;
}
QRect centered_window(const QRect &area, const QSize &size) {
    const QSize fitted(std::min(area.width(), size.width()), std::min(area.height(), size.height()));
    return {area.topLeft() +
                QPoint((area.width() - fitted.width()) / 2, (area.height() - fitted.height()) / 2),
            fitted};
}
void present_window(QWindow &window, const QPoint &anchor, std::function<void()> native_focus) {
    auto *screen = resolve_display(QStringLiteral("auto"), anchor);
    if (screen == nullptr) {
        return;
    }
    const bool maximized = window.windowState() == Qt::WindowMaximized;
    window.showNormal();
    window.setScreen(screen);
    window.setGeometry(centered_window(screen->availableGeometry(), window.size()));
    if (maximized) {
        window.showMaximized();
    }
    request_activation(window, std::move(native_focus));
}
DisplayCatalog::DisplayCatalog() {
    for (auto *screen : QGuiApplication::screens()) {
        observe(screen);
    }
    connect(qGuiApp, &QGuiApplication::screenAdded, this, [this](QScreen *screen) {
        observe(screen);
        emit changed();
    });
    connect(qGuiApp, &QGuiApplication::screenRemoved, this, &DisplayCatalog::changed);
}
void DisplayCatalog::observe(QScreen *screen) {
    connect(screen, &QScreen::geometryChanged, this, &DisplayCatalog::changed);
    connect(screen, &QScreen::availableGeometryChanged, this, &DisplayCatalog::changed);
}
QVariantList DisplayCatalog::screens() const {
    QRect bounds;
    const auto screens = QGuiApplication::screens();
    for (const auto *screen : screens) {
        bounds = bounds.united(screen->geometry());
    }
    QVariantList result;
    for (int index = 0; index < screens.size(); ++index) {
        const auto *screen = screens.at(index);
        const auto rect = screen->geometry();
        result.append(QVariantMap{{QStringLiteral("id"), display_id(*screen)},
                                  {QStringLiteral("name"), screen->name()},
                                  {QStringLiteral("aspect"), double(bounds.width()) / bounds.height()},
                                  {QStringLiteral("number"), index + 1},
                                  {QStringLiteral("x"), double(rect.x() - bounds.x()) / bounds.width()},
                                  {QStringLiteral("y"), double(rect.y() - bounds.y()) / bounds.height()},
                                  {QStringLiteral("width"), double(rect.width()) / bounds.width()},
                                  {QStringLiteral("height"), double(rect.height()) / bounds.height()}});
    }
    return result;
}
} // namespace manager::presentation
