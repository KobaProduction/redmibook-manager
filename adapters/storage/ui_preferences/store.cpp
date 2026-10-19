#include "adapters/storage/ui_preferences/store.hpp"
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLockFile>
#include <QSaveFile>
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
        return {60, 165};
    std::vector<unsigned int> rates;
    for (const auto rate : ui.value(QStringLiteral("refreshCycleRates")).toArray()) {
        const int hz = rate.toInt(-1);
        if (hz < 2 || hz > 1000)
            return {};
        rates.push_back(static_cast<unsigned int>(hz));
    }
    return rates;
}

application::shortcuts::BindingAction read_binding(const QJsonObject &ui, const QString &key) {
    if (!ui.contains(key))
        return application::shortcuts::BindingAction::Default;
    const int action = ui.value(key).toInt(-1);
    return static_cast<application::shortcuts::BindingAction>(
        application::shortcuts::valid_binding_action(action) ? action : 255);
}

application::UiPreferences decode(const QJsonObject &object) {
    const auto ui = object.value(QStringLiteral("ui")).toObject();
    return {.language = ui.value(QStringLiteral("language")).toString(QStringLiteral("system")).toStdString(),
            .theme = ui.value(QStringLiteral("theme")).toString(QStringLiteral("system")).toStdString(),
            .revision = static_cast<std::uint64_t>(object.value(QStringLiteral("revision")).toInteger(0)),
            .osd_position =
                ui.value(QStringLiteral("osdPosition")).toString(QStringLiteral("top-left")).toStdString(),
            .tray_autostart = ui.value(QStringLiteral("trayAutostart")).toBool(true),
            .osd_screen =
                ui.value(QStringLiteral("osdScreen")).toString(QStringLiteral("auto")).toStdString(),
            .design = ui.value(QStringLiteral("design")).toString(QStringLiteral("fluent")).toStdString(),
            .auto_updates = ui.value(QStringLiteral("autoUpdates")).toBool(true),
            .rounded_corners = ui.value(QStringLiteral("roundedCorners")).toBool(true),
            .refresh_cycle_rates = read_refresh_rates(ui),
            .fn_s_action = read_binding(ui, QStringLiteral("fnSAction")),
            .fn_k_action = read_binding(ui, QStringLiteral("fnKAction"))};
}

std::optional<QJsonValue> encode_preference(std::string_view key, std::string_view value) {
    const auto text = QString::fromUtf8(value.data(), static_cast<qsizetype>(value.size()));
    if (key == "refreshCycleRates") {
        const auto document = QJsonDocument::fromJson(text.toUtf8());
        return document.isArray() ? std::optional<QJsonValue>(document.array()) : std::nullopt;
    }
    if (key == "fnSAction" || key == "fnKAction") {
        bool valid = false;
        const int action = text.toInt(&valid);
        return valid && application::shortcuts::valid_binding_action(action)
                   ? std::optional<QJsonValue>(action)
                   : std::nullopt;
    }
    const bool boolean = key == "trayAutostart" || key == "autoUpdates" || key == "roundedCorners";
    if (boolean)
        return value == "true" || value == "false" ? std::optional<QJsonValue>(value == "true")
                                                   : std::nullopt;
    return QJsonValue(text);
}

bool write_preference(QJsonObject &object, std::string_view key, std::string_view value) {
    const auto encoded = encode_preference(key, value);
    if (!encoded)
        return false;
    auto ui = object.value(QStringLiteral("ui")).toObject();
    ui.insert(QString::fromUtf8(key.data(), static_cast<qsizetype>(key.size())), *encoded);
    object.insert(QStringLiteral("ui"), ui);
    return application::valid_preferences(decode(object));
}
} // namespace

UiPreferenceStore::UiPreferenceStore(QString directory) : directory_(std::move(directory)) {
    QDir().mkpath(directory_);
    watcher_.addPath(directory_);
    notify_change();
    connect(&watcher_, &QFileSystemWatcher::directoryChanged, this, &UiPreferenceStore::notify_change);
    connect(&watcher_, &QFileSystemWatcher::fileChanged, this, &UiPreferenceStore::notify_change);
}

void UiPreferenceStore::notify_change() {
    if (QFile::exists(path()) && !watcher_.files().contains(path())) {
        watcher_.addPath(path());
    }
    if (changed_) {
        changed_();
    }
}

void UiPreferenceStore::observe(std::function<void()> changed) {
    changed_ = std::move(changed);
}

QString UiPreferenceStore::path() const {
    return QDir(directory_).filePath(QStringLiteral("settings.json"));
}

std::optional<application::UiPreferences> UiPreferenceStore::read() const {
    const auto object = read_object(path());
    if (!object || object->value(QStringLiteral("schema")).toInt(1) != 1) {
        return std::nullopt;
    }
    const auto value = decode(*object);
    return application::valid_preferences(value) ? std::optional(value) : std::nullopt;
}

bool UiPreferenceStore::update(std::string_view key, std::string_view value) {
    QLockFile lock(path() + QStringLiteral(".lock"));
    if (!lock.tryLock(100)) {
        return false;
    }
    auto object = read_object(path());
    if (!object || !read() || !write_preference(*object, key, value)) {
        return false;
    }
    object->insert(QStringLiteral("schema"), 1);
    object->insert(QStringLiteral("revision"), object->value(QStringLiteral("revision")).toInteger() + 1);
    QSaveFile output(path());
    return output.open(QIODevice::WriteOnly) && output.write(QJsonDocument(*object).toJson()) != -1 &&
           output.commit();
}
} // namespace manager::adapters
