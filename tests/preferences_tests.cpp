#include "adapters/storage/ui_preferences/store.hpp"
#include "application/runtime/read_status.hpp"
#include "apps/desktop/preview_presets.hpp"
#include "presentation/common/preferences.hpp"
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>

namespace {
bool translation_contract(manager::presentation::Preferences &preferences) {
    if (!preferences.set_language(QStringLiteral("ru"))) {
        return false;
    }
    if (QCoreApplication::translate("Main", "Overview") != QStringLiteral("Обзор")) {
        return false;
    }
    if (!preferences.set_language(QStringLiteral("en"))) {
        return false;
    }
    if (QCoreApplication::translate("Main", "Overview") != QStringLiteral("Overview")) {
        return false;
    }
    return !preferences.set_language(QStringLiteral("unsupported")) &&
           preferences.language() == QStringLiteral("en");
}

bool appearance_contract(manager::presentation::Preferences &preferences) {
    if (!preferences.set_theme(QStringLiteral("dark"))) {
        return false;
    }
    if (QApplication::palette().color(QPalette::Window).lightness() >= 128) {
        return false;
    }
    if (!preferences.set_theme(QStringLiteral("light"))) {
        return false;
    }
    if (QApplication::palette().color(QPalette::Window).lightness() < 128) {
        return false;
    }
    return preferences.set_theme(QStringLiteral("system"));
}

bool reject_corrupt_file(const QString &directory, manager::adapters::UiPreferenceStore &first) {
    QFile corrupt(directory + QStringLiteral("/settings.json"));
    if (!corrupt.open(QIODevice::WriteOnly) || corrupt.write("not JSON") == -1) {
        return false;
    }
    corrupt.close();
    return !first.read() && !first.update("language", "en");
}
bool storage_contract(const QString &directory) {
    manager::adapters::UiPreferenceStore first(directory), second(directory);
    using manager::application::shortcuts::BindingAction;
    if (!first.update("language", "ru") || !second.update("theme", "dark") ||
        !first.update("vendorKeyAction", "2") || !second.update("fnSAction", "3") ||
        first.update("vendorKeyAction", "4"))
        return false;
    const auto saved = first.read();
    if (!saved || saved->vendor_key_action != BindingAction::None ||
        saved->fn_s_action != BindingAction::OpenTray || saved->language != "ru" || saved->theme != "dark" ||
        !second.update("vendorKeyAction", "3"))
        return false;
    const auto updated = first.read();
    return updated && updated->vendor_key_action == BindingAction::OpenTray &&
           reject_corrupt_file(directory, first);
}

bool write_reset_fixture(QTemporaryDir &directory) {
    QFile file(directory.filePath("settings.json"));
    if (!file.open(QIODevice::WriteOnly) ||
        file.write(
            R"({"schema":1,"revision":40,"other":{"keep":true},"ui":{"language":"en","theme":"dark","trayAutostart":false,"batteryIntervalMs":5000,"fnKAction":3,"refreshCycleRates":[60,120]}})") <
            0)
        return false;
    file.close();
    return true;
}
bool check_reset_disk(const QString &directory, std::uint64_t revision) {
    QFile file(QDir(directory).filePath("settings.json"));
    manager::adapters::UiPreferenceStore reader(directory);
    const auto after = reader.read();
    if (!after || after->revision != revision + 1 || after->language != "system" ||
        after->theme != "system" || !after->tray_autostart || after->battery_interval_ms != 1000 ||
        !after->tray_presets.empty() || after->refresh_cycle_rates != std::vector<unsigned int>{60, 120} ||
        !file.open(QIODevice::ReadOnly))
        return false;
    const auto document = QJsonDocument::fromJson(file.readAll()).object();
    file.close();
    return document.value("ui").toObject().size() == 1 &&
           document.value("other").toObject().value("keep").toBool();
}
bool reset_contract() {
    QTemporaryDir directory;
    if (!write_reset_fixture(directory))
        return false;
    manager::adapters::UiPreferenceStore store(directory.path());
    manager::presentation::Preferences preferences(store);
    if (!preferences.set_tray_presets(manager::preview::demo_presets()) || store.update("fnKAction", "1"))
        return false;
    const auto before = store.read();
    if (!before)
        return false;
    if (!preferences.reset_defaults() || preferences.language() != QStringLiteral("system"))
        return false;
    return check_reset_disk(directory.path(), before->revision);
}

std::vector<manager::application::presets::Preset> large_presets() {
    using namespace manager::application::presets;
    std::vector<Preset> presets;
    for (int p = 0; p < 16; ++p) {
        Preset preset{std::to_string(p), "Preset", true, {}};
        for (int b = 0; b < 8; ++b) {
            Button button{std::to_string(b), "Button", "balanced", {}};
            for (int a = 0; a < 8; ++a)
                button.actions.push_back({Kind::Performance, "1"});
            preset.buttons.push_back(button);
        }
        presets.push_back(preset);
    }
    return presets;
}
bool oversized_save_contract(manager::adapters::UiPreferenceStore &store, const QString &directory) {
    QFile file(directory + "/settings.json");
    if (!file.open(QIODevice::WriteOnly))
        return false;
    const QByteArray previous = QJsonDocument(QJsonObject{{"schema", 1}, {"other", QString(40000, 'x')}})
                                    .toJson(QJsonDocument::Compact);
    if (file.write(previous) != previous.size())
        return false;
    file.close();
    if (store.save_tray_presets(large_presets()) || !file.open(QIODevice::ReadOnly))
        return false;
    return file.readAll() == previous && store.read().has_value();
}
bool large_presets_contract() {
    QTemporaryDir directory;
    manager::adapters::UiPreferenceStore store(directory.path());
    const auto presets = large_presets();
    if (!store.save_tray_presets(presets))
        return false;
    const auto loaded = store.read();
    return loaded && loaded->tray_presets == presets && oversized_save_contract(store, directory.path());
}

bool starter_defaults_contract() {
    QTemporaryDir directory;
    manager::adapters::UiPreferenceStore store(directory.path());
    const auto catalog = manager::preview::demo_catalog();
    if (!store.update("refreshCycleRates", "[60,120]") || !store.configure_starter_defaults(catalog))
        return false;
    const auto initial = store.read();
    if (!initial || initial->tray_presets.size() != 1 ||
        initial->tray_presets.front().buttons.back().actions.front().value != "120")
        return false;
    if (!store.save_tray_presets({}) || !store.configure_starter_defaults(catalog))
        return false;
    const auto deleted = store.read();
    if (!deleted || !deleted->tray_presets.empty() || !store.reset_defaults())
        return false;
    const auto reset = store.read();
    return reset && reset->tray_presets == initial->tray_presets &&
           reset->refresh_cycle_rates == initial->refresh_cycle_rates;
}
class UnavailableDiagnostic final : public manager::application::DiagnosticPort {
  public:
    manager::application::DiagnosticReply probe() const override {
        return {};
    }
};
} // namespace

bool contracts(manager::presentation::Preferences &preferences, const QString &directory) {
    const bool translated = translation_contract(preferences);
    const bool appearance = appearance_contract(preferences);
    const bool storage = storage_contract(directory);
    const bool sync = true; // Live synchronization is covered by preference-channel-contract.
    const bool reset = reset_contract() && large_presets_contract() && starter_defaults_contract();
    if (!translated || !appearance || !storage || !sync || !reset)
        qCritical("preferences: translation=%d appearance=%d storage=%d sync=%d reset=%d", translated,
                  appearance, storage, sync, reset);
    return translated && appearance && storage && sync && reset;
}

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    app.setStyle(QStringLiteral("Fusion"));
    QTemporaryDir directory;
    manager::adapters::UiPreferenceStore store(directory.path());
    manager::presentation::Preferences preferences(store);
    const UnavailableDiagnostic diagnostic;
    const manager::application::ReadRuntimeStatus read(diagnostic);
    return contracts(preferences, directory.path()) && !read.execute().connected ? 0 : 1;
}
