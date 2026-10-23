#include "presentation/common/preferences.hpp"
#include <QApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLocale>
#include <QQmlEngine>
#include <QStyleHints>
#include <algorithm>
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
    store_.observe_connection([this] { emit connectionChanged(); });
    connect(this, &Preferences::languageChanged, this, &Preferences::connectionChanged);
    store_.observe_error([this] {
        emit errorOccurred(
            tr("The background runtime could not save preferences. Changes may not survive a restart."));
    });
    connect(&appearance_, &ThemeController::changed, this, &Preferences::paletteChanged);
    refresh();
}

Preferences::~Preferences() {
    store_.observe({});
    store_.observe_connection({});
    store_.observe_error({});
}

QString Preferences::connectionText() const {
    switch (store_.connection()) {
    case application::PreferenceConnection::Ready:
        return {};
    case application::PreferenceConnection::Connecting:
        return tr("Connecting to the background runtime…");
    case application::PreferenceConnection::Incompatible:
        return tr("Background runtime version is incompatible. Restart the components.");
    case application::PreferenceConnection::Unavailable:
        return tr("Background runtime is unavailable. Controls are disabled until reconnection.");
    }
    return {};
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

std::vector<unsigned int> Preferences::refresh_cycle_rates() const {
    return value_.refresh_cycle_rates;
}
bool Preferences::set_refresh_cycle_rates(const std::vector<unsigned int> &rates) {
    if (!application::display::valid_refresh_selection(rates))
        return false;
    QJsonArray values;
    for (const auto rate : rates)
        values.append(static_cast<int>(rate));
    if (!store_.update("refreshCycleRates",
                       QJsonDocument(values).toJson(QJsonDocument::Compact).toStdString()))
        return false;
    refresh();
    return true;
}
int Preferences::fn_s_action() const {
    return static_cast<int>(value_.fn_s_action);
}

int Preferences::vendor_key_action() const {
    return static_cast<int>(value_.vendor_key_action);
}
bool Preferences::set_vendor_key_action(int action) {
    if (!application::shortcuts::valid_binding_action(action) ||
        !store_.update("vendorKeyAction", std::to_string(action)))
        return false;
    refresh();
    return true;
}
bool Preferences::set_fn_s_action(int action) {
    if (!application::shortcuts::valid_binding_action(action) ||
        !store_.update("fnSAction", std::to_string(action)))
        return false;
    refresh();
    return true;
}
bool Preferences::set_battery_interval_ms(int interval) {
    if (!application::valid_battery_interval(interval) ||
        !store_.update("batteryIntervalMs", std::to_string(interval)))
        return false;
    refresh();
    return true;
}
bool Preferences::tray_section_enabled(std::string_view id) const {
    return std::find(value_.tray_sections.begin(), value_.tray_sections.end(), id) !=
           value_.tray_sections.end();
}
bool Preferences::set_tray_section_enabled(const QString &id, bool enabled) {
    auto sections = value_.tray_sections;
    const auto key = id.toStdString();
    std::erase(sections, key);
    if (enabled)
        sections.push_back(key);
    if (!application::valid_tray_sections(sections))
        return false;
    QJsonArray array;
    for (const auto &section : sections)
        array.append(QString::fromStdString(section));
    if (!store_.update("traySections", QJsonDocument(array).toJson(QJsonDocument::Compact).toStdString()))
        return false;
    refresh();
    return true;
}

void Preferences::refresh() {
    const auto next = store_.read();
    if (!next || (initialized_ && next->revision == value_.revision))
        return;
    if (!apply_language(QString::fromStdString(next->language)))
        return;
    const bool theme_changed = !initialized_ || next->theme != value_.theme;
    value_ = *next;
    initialized_ = true;
    if (theme_changed)
        apply_theme();
    emit changed();
}

bool Preferences::set_language(const QString &value) {
    if (!store_.update("language", value.toStdString())) {
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
