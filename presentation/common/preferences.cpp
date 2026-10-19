#include "presentation/common/preferences.hpp"
#include <QApplication>
#include <QLocale>
#include <QQmlEngine>
#include <QStyleHints>
#include <utility>

namespace manager::presentation {
namespace {
QString resolve_language(const QString &requested) {
    if (requested != QStringLiteral("system")) {
        return requested;
    }
    for (const auto &language : QLocale::system().uiLanguages()) {
        const auto candidate = language.section(QLatin1Char('-'), 0, 0);
        if (candidate == QStringLiteral("en") || candidate == QStringLiteral("ru")) {
            return candidate;
        }
    }
    return QStringLiteral("en");
}
} // namespace

Preferences::Preferences(application::PreferencePort &store, QObject *parent)
    : QObject(parent), store_(store) {
    store_.observe([this] { refresh(); });
    connect(&appearance_, &ThemeController::changed, this, &Preferences::paletteChanged);
    refresh();
}

Preferences::~Preferences() {
    store_.observe({});
}

QString Preferences::language() const {
    return QString::fromStdString(value_.language);
}
QString Preferences::theme() const {
    return QString::fromStdString(value_.theme);
}

QString Preferences::osd_position() const {
    return QString::fromStdString(value_.osd_position);
}
bool Preferences::tray_autostart() const {
    return value_.tray_autostart;
}
bool Preferences::set_osd_position(const QString &value) {
    if (!store_.update("osdPosition", value.toStdString())) {
        return false;
    }
    refresh();
    return true;
}
bool Preferences::set_tray_autostart(bool enabled) {
    if (!store_.update("trayAutostart", enabled ? "true" : "false")) {
        return false;
    }
    refresh();
    return true;
}

QString Preferences::osd_screen() const {
    return QString::fromStdString(value_.osd_screen);
}
QString Preferences::design() const {
    return QString::fromStdString(value_.design);
}
bool Preferences::auto_updates() const {
    return value_.auto_updates;
}
bool Preferences::set_osd_screen(const QString &value) {
    if (!store_.update("osdScreen", value.toStdString())) {
        return false;
    }
    refresh();
    return true;
}
bool Preferences::set_design(const QString &value) {
    if (!store_.update("design", value.toStdString())) {
        return false;
    }
    refresh();
    return true;
}
bool Preferences::set_auto_updates(bool enabled) {
    if (!store_.update("autoUpdates", enabled ? "true" : "false")) {
        return false;
    }
    refresh();
    return true;
}

bool Preferences::apply_language(const QString &requested) {
    const auto resolved = resolve_language(requested);
    if (initialized_ && resolved == effective_language_) {
        return true;
    }
    auto next = std::make_unique<QTranslator>();
    if (resolved == QStringLiteral("ru") && !next->load(QStringLiteral(":/translations/manager_ru.qm"))) {
        return false;
    }
    if (translator_) {
        QCoreApplication::removeTranslator(translator_.get());
    }
    translator_ = std::move(next);
    if (resolved == QStringLiteral("ru")) {
        QCoreApplication::installTranslator(translator_.get());
    }
    effective_language_ = resolved;
    emit languageChanged();
    return true;
}

void Preferences::apply_theme() {
    appearance_.set_mode(theme());
}

void Preferences::refresh() {
    const auto next = store_.read();
    if (!next || (initialized_ && next->revision == value_.revision)) {
        return;
    }
    if (!apply_language(QString::fromStdString(next->language))) {
        return;
    }
    value_ = *next;
    initialized_ = true;
    apply_theme();
    emit changed();
}

bool Preferences::set_language(const QString &value) {
    if (!apply_language(value) || !store_.update("language", value.toStdString())) {
        apply_language(language());
        return false;
    }
    refresh();
    return true;
}

bool Preferences::set_theme(const QString &value) {
    if (!store_.update("theme", value.toStdString())) {
        return false;
    }
    refresh();
    return true;
}

bool Preferences::rounded_corners() const {
    return value_.rounded_corners;
}
bool Preferences::set_rounded_corners(bool enabled) {
    if (!store_.update("roundedCorners", enabled ? "true" : "false")) {
        return false;
    }
    refresh();
    return true;
}
QPalette Preferences::palette() const {
    return QGuiApplication::palette();
}

void Preferences::attach_engine(QQmlEngine &engine) {
    connect(this, &Preferences::languageChanged, &engine, &QQmlEngine::retranslate);
}
} // namespace manager::presentation
