#include "adapters/storage/ui_preferences/store.hpp"
#include "application/runtime/read_status.hpp"
#include "presentation/common/preferences.hpp"
#include "tests/preferences_sync.hpp"
#include <QApplication>
#include <QFile>
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

bool storage_contract(const QString &directory) {
    manager::adapters::UiPreferenceStore first(directory);
    manager::adapters::UiPreferenceStore second(directory);
    if (!first.update("language", "ru") || !second.update("theme", "dark")) {
        return false;
    }
    const auto saved = first.read();
    if (!saved || saved->language != "ru" || saved->theme != "dark") {
        return false;
    }
    QFile corrupt(directory + QStringLiteral("/settings.json"));
    if (!corrupt.open(QIODevice::WriteOnly) || corrupt.write("not JSON") == -1) {
        return false;
    }
    corrupt.close();
    return !first.read() && !first.update("language", "en");
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
    const bool sync = preferences_sync_contract();
    if (!translated || !appearance || !storage || !sync)
        qCritical("preferences: translation=%d appearance=%d storage=%d sync=%d", translated, appearance,
                  storage, sync);
    return translated && appearance && storage && sync;
}

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    app.setStyle(QStringLiteral("Fusion"));
    if (app.arguments().size() == 3 && app.arguments().at(1) == QStringLiteral("--observe"))
        return observe_preferences(app, app.arguments().at(2));
    QTemporaryDir directory;
    manager::adapters::UiPreferenceStore store(directory.path());
    manager::presentation::Preferences preferences(store);
    const UnavailableDiagnostic diagnostic;
    const manager::application::ReadRuntimeStatus read(diagnostic);
    return contracts(preferences, directory.path()) && !read.execute().connected ? 0 : 1;
}
