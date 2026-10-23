#include "adapters/storage/ui_preferences/store.hpp"
#include "adapters/storage/ui_preferences/presets_codec.hpp"
#include "application/presets/starter.hpp"
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QtConcurrentRun>
#include <utility>

namespace manager::adapters {
namespace {
std::optional<QJsonObject> read_object(const QString &path) {
    QFile file(path);
    if (!file.exists()) {
        return QJsonObject{};
    }
    if (!file.open(QIODevice::ReadOnly) || file.size() > 65536) {
        return std::nullopt;
    }
    QJsonParseError error{};
    const auto document = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) {
        return std::nullopt;
    }
    return document.object();
}

std::vector<unsigned int> read_refresh_rates(const QJsonObject &ui) {
    if (!ui.contains(QStringLiteral("refreshCycleRates")))
        return {};
    std::vector<unsigned int> rates;
    for (const auto rate : ui.value(QStringLiteral("refreshCycleRates")).toArray()) {
        const int hz = rate.toInt(-1);
        if (hz < 2 || hz > 1000)
            return {0};
        rates.push_back(static_cast<unsigned int>(hz));
    }
    return rates;
}

application::shortcuts::BindingAction read_binding(const QJsonObject &ui, const QString &key) {
    if (!ui.contains(key))
        return application::shortcuts::BindingAction::Default;
    const int action = ui.value(key).toInt(-1);
    return application::shortcuts::valid_binding_action(action)
               ? static_cast<application::shortcuts::BindingAction>(action)
               : application::shortcuts::BindingAction::Invalid;
}

std::vector<std::string> read_tray_sections(const QJsonObject &ui) {
    if (!ui.contains("traySections"))
        return application::UiPreferences{}.tray_sections;
    if (!ui.value("traySections").isArray())
        return {"invalid"};
    std::vector<std::string> result;
    for (const auto item : ui.value("traySections").toArray())
        result.push_back(item.toString().toStdString());
    return result;
}
int read_battery_interval(const QJsonObject &ui) {
    const int interval = ui.value(QStringLiteral("batteryIntervalMs")).toInt(1000);
    return interval == 5 ? 5000 : interval;
}

application::UiPreferences decode(const QJsonObject &object) {
    const auto ui = object.value(QStringLiteral("ui")).toObject();
    return {
        .language = ui.value(QStringLiteral("language")).toString(QStringLiteral("system")).toStdString(),
        .theme = ui.value(QStringLiteral("theme")).toString(QStringLiteral("system")).toStdString(),
        .revision = static_cast<std::uint64_t>(object.value(QStringLiteral("revision")).toInteger(0)),
        .osd_position =
            ui.value(QStringLiteral("osdPosition")).toString(QStringLiteral("top-left")).toStdString(),
        .tray_autostart = ui.value(QStringLiteral("trayAutostart")).toBool(true),
        .osd_screen = ui.value(QStringLiteral("osdScreen")).toString(QStringLiteral("auto")).toStdString(),
        .design = ui.value(QStringLiteral("design")).toString(QStringLiteral("fluent")).toStdString(),
        .auto_updates = ui.value(QStringLiteral("autoUpdates")).toBool(true),
        .rounded_corners = ui.value(QStringLiteral("roundedCorners")).toBool(true),
        .osd_sound = ui.value(QStringLiteral("osdSound")).toBool(true),
        .osd_enabled = ui.value(QStringLiteral("osdEnabled")).toBool(true),
        .refresh_cycle_rates = read_refresh_rates(ui),
        .fn_s_action = read_binding(ui, QStringLiteral("fnSAction")),
        .vendor_key_action = read_binding(ui, QStringLiteral("vendorKeyAction")),
        .battery_interval_ms = read_battery_interval(ui),
        .tray_sections = read_tray_sections(ui),
        .tray_presets = decode_presets(ui.value("trayPresets"))
                            .value_or(std::vector<application::presets::Preset>{{"invalid", {}, true, {}}})};
}

std::optional<QJsonValue> encode_integer(const QString &text, bool (*accept)(int)) {
    bool parsed{};
    const int value = text.toInt(&parsed);
    return parsed && accept(value) ? std::optional<QJsonValue>(value) : std::nullopt;
}
std::optional<QJsonValue> encode_preference(std::string_view key, const QString &text) {
    if (key == "batteryIntervalMs")
        return encode_integer(text, application::valid_battery_interval);
    if (key == "refreshCycleRates" || key == "traySections" || key == "trayPresets") {
        const auto document = QJsonDocument::fromJson(text.toUtf8());
        return document.isArray() ? std::optional<QJsonValue>(document.array()) : std::nullopt;
    }
    // Legacy fnKAction is ignored on read and must never become an active binding again.
    if (key == "fnKAction")
        return std::nullopt;
    if (key == "fnSAction" || key == "vendorKeyAction")
        return encode_integer(text, application::shortcuts::valid_binding_action);
    const bool boolean = key == "trayAutostart" || key == "autoUpdates" || key == "roundedCorners" ||
                         key == "osdSound" || key == "osdEnabled";
    if (boolean)
        return text == QStringLiteral("true") || text == QStringLiteral("false")
                   ? std::optional<QJsonValue>(text == QStringLiteral("true"))
                   : std::nullopt;
    return QJsonValue(text);
}

bool write_preference(QJsonObject &object, std::string_view key, std::string_view value) {
    const auto encoded =
        encode_preference(key, QString::fromUtf8(value.data(), static_cast<qsizetype>(value.size())));
    if (!encoded)
        return false;
    auto ui = object.value(QStringLiteral("ui")).toObject();
    ui.insert(QString::fromUtf8(key.data(), static_cast<qsizetype>(key.size())), *encoded);
    if (key == "osdPosition")
        ui.insert(QStringLiteral("osdScreen"), QStringLiteral("auto"));
    object.insert(QStringLiteral("ui"), ui);
    return application::valid_preferences(decode(object));
}
} // namespace

UiPreferenceStore::UiPreferenceStore(QString directory, bool authority)
    : directory_(std::move(directory)), authority_(authority) {
    if (authority_) {
        QDir().mkpath(directory_);
        memory_ = read_object(path());
        save_timer_.setSingleShot(true);
        save_timer_.setInterval(250);
        connect(&save_timer_, &QTimer::timeout, this, &UiPreferenceStore::flush);
        connect(&saving_, &QFutureWatcher<bool>::finished, this, &UiPreferenceStore::save_finished);
        return;
    }
    QDir().mkpath(directory_);
}

void UiPreferenceStore::save_finished() {

    if (!saving_.result()) {
        dirty_ = true;
        if (!save_error_reported_ && error_)
            error_();
        save_error_reported_ = true;
        save_timer_.start(10000);
    } else {
        save_error_reported_ = false;
        if (dirty_)
            save_timer_.start(250);
    }
}

void UiPreferenceStore::notify_change() {
    if (changed_)
        changed_();
}

void UiPreferenceStore::observe(std::function<void()> changed) {
    changed_ = std::move(changed);
}

QString UiPreferenceStore::path() const {
    return QDir(directory_).filePath(QStringLiteral("settings.json"));
}

std::optional<application::UiPreferences> UiPreferenceStore::read() const {
    if (authority_)
        return memory_ && application::valid_preferences(decode(*memory_)) ? std::optional(decode(*memory_))
                                                                           : std::nullopt;
    const auto object = read_object(path());
    if (!object || object->value(QStringLiteral("schema")).toInt(1) != 1)
        return std::nullopt;
    const auto value = decode(*object);
    return application::valid_preferences(value) ? std::optional(value) : std::nullopt;
}

namespace {
void initialize_starter_presets(QJsonObject &ui, const application::presets::Catalog &catalog,
                                std::vector<application::presets::Preset> presets) {
    std::ranges::for_each(presets, application::presets::migrate_starter);
    if (ui.value(QStringLiteral("starterPresetVersion")).toInt() == 0 && presets.size() < 16 &&
        std::ranges::none_of(presets, [](const auto &preset) { return preset.id == "starter-energy"; }))
        presets.push_back(application::presets::starter(catalog, read_refresh_rates(ui)));
    ui.insert(QStringLiteral("trayPresets"), encode_presets(presets));
    ui.insert(QStringLiteral("starterPresetVersion"), application::presets::starter_version);
}
bool save_object(const QString &path, const QJsonObject &object) {
    QSaveFile output(path);
    const auto bytes = QJsonDocument(object).toJson(QJsonDocument::Compact);
    if (bytes.size() > 65536)
        return false;
    if (!output.open(QIODevice::WriteOnly) || output.write(bytes) != bytes.size() || !output.commit()) {
        qWarning().noquote() << "preferences-save-failed:" << output.errorString();
        return false;
    }
    return true;
}
} // namespace

bool UiPreferenceStore::update(std::string_view key, std::string_view value) {
    auto object = authority_ ? memory_ : read_object(path());
    if (!object || object->value(QStringLiteral("schema")).toInt(1) != 1 ||
        !application::valid_preferences(decode(*object)))
        return false;
    const auto before = object->value(QStringLiteral("ui"));
    if (!write_preference(*object, key, value))
        return false;
    if (object->value(QStringLiteral("ui")) == before)
        return true;
    object->insert(QStringLiteral("schema"), 1);
    object->insert(QStringLiteral("revision"), object->value(QStringLiteral("revision")).toInteger() + 1);
    return commit_object(*object);
}

bool UiPreferenceStore::reset_defaults() {
    auto object = authority_ ? memory_ : read_object(path());
    if (!object || object->value(QStringLiteral("schema")).toInt(1) != 1)
        return false;
    const auto previous = object->value(QStringLiteral("ui")).toObject();
    QJsonObject defaults;
    if (previous.contains(QStringLiteral("refreshCycleRates")) &&
        (read_refresh_rates(previous).empty() ||
         application::display::valid_refresh_selection(read_refresh_rates(previous))))
        defaults.insert(QStringLiteral("refreshCycleRates"),
                        previous.value(QStringLiteral("refreshCycleRates")));
    if (starter_catalog_) {
        defaults.insert(
            QStringLiteral("trayPresets"),
            encode_presets({application::presets::starter(*starter_catalog_, read_refresh_rates(defaults))}));
        defaults.insert(QStringLiteral("starterPresetVersion"), application::presets::starter_version);
    }
    object->insert(QStringLiteral("ui"), defaults);
    object->insert(QStringLiteral("schema"), 1);
    object->insert(QStringLiteral("revision"), object->value(QStringLiteral("revision")).toInteger() + 1);
    return commit_object(*object);
}
bool UiPreferenceStore::configure_starter_defaults(application::presets::Catalog catalog) {
    starter_catalog_ = std::move(catalog);
    auto object = authority_ ? memory_ : read_object(path());
    if (!object || object->value(QStringLiteral("schema")).toInt(1) != 1 ||
        !application::valid_preferences(decode(*object)))
        return false;
    auto ui = object->value(QStringLiteral("ui")).toObject();
    const int version = ui.value(QStringLiteral("starterPresetVersion")).toInt();
    if (version >= application::presets::starter_version)
        return true;
    initialize_starter_presets(ui, *starter_catalog_, decode(*object).tray_presets);
    object->insert(QStringLiteral("ui"), ui);
    object->insert(QStringLiteral("revision"), object->value(QStringLiteral("revision")).toInteger() + 1);
    return commit_object(*object);
}
UiPreferenceStore::~UiPreferenceStore() {
    save_timer_.stop();
    if (!authority_)
        return;
    saving_.waitForFinished();
    if (saving_.future().resultCount() > 0 && !saving_.result())
        dirty_ = true;
    if (dirty_ && memory_) {
        const auto target = path();
        const auto object = *memory_;
        auto final_write = QtConcurrent::run([target, object] { return save_object(target, object); });
        final_write.waitForFinished();
        dirty_ = !final_write.result();
        if (dirty_)
            qCritical("preferences-final-save-failed");
    }
}
QByteArray UiPreferenceStore::snapshot() const {
    return memory_ ? QJsonDocument(*memory_).toJson(QJsonDocument::Compact) : QByteArray{};
}
std::optional<application::UiPreferences> UiPreferenceStore::decode_snapshot(const QByteArray &bytes) {
    if (bytes.size() > 65536)
        return {};
    QJsonParseError error;
    const auto doc = QJsonDocument::fromJson(bytes, &error);
    if (error.error != QJsonParseError::NoError || !doc.isObject() ||
        doc.object().value("schema").toInt(1) != 1)
        return {};
    const auto value = decode(doc.object());
    return application::valid_preferences(value) ? std::optional(value) : std::nullopt;
}
bool UiPreferenceStore::commit_object(const QJsonObject &object) {
    if (!authority_) {
        const bool saved = save_object(path(), object);
        if (saved)
            notify_change();
        return saved;
    }
    if (QJsonDocument(object).toJson(QJsonDocument::Compact).size() > 65536)
        return false;
    memory_ = object;
    dirty_ = true;
    save_timer_.start(250);
    notify_change();
    return true;
}
void UiPreferenceStore::flush() {
    if (!dirty_ || saving_.isRunning() || !memory_)
        return;
    dirty_ = false;
    const auto target = path();
    const auto object = *memory_;
    saving_.setFuture(QtConcurrent::run([target, object] { return save_object(target, object); }));
}
} // namespace manager::adapters
