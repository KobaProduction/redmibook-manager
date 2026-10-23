#include "presentation/manager_model.hpp"
#include <QPointer>
#include <QStringList>
#include <QTimer>
#include <array>

namespace manager::presentation {
QVariantList ManagerModel::traySections() const {
    const QStringList labels{tr("Performance mode"), tr("Screen refresh rate"), tr("Charge limit"),
                             tr("Battery power"),    tr("Microphone"),          tr("Connection status")};
    QVariantList result;
    for (std::size_t index = 0; index < application::tray_section_ids.size(); ++index) {
        const auto id = application::tray_section_ids[index];
        result.append(QVariantMap{
            {QStringLiteral("id"), QString::fromUtf8(id.data(), static_cast<qsizetype>(id.size()))},
            {QStringLiteral("label"), labels.at(static_cast<qsizetype>(index))},
            {QStringLiteral("enabled"), preferences_.tray_section_enabled(id)}});
    }
    return result;
}
namespace {
const std::array<QString, 3> languages{QStringLiteral("system"), QStringLiteral("en"), QStringLiteral("ru")};
const std::array<QString, 3> themes{QStringLiteral("system"), QStringLiteral("light"),
                                    QStringLiteral("dark")};
int index_of(const std::array<QString, 3> &values, const QString &value) {
    for (std::size_t index = 0; index < values.size(); ++index) {
        if (values[index] == value) {
            return static_cast<int>(index);
        }
    }
    return 0;
}
} // namespace

ManagerModel::ManagerModel(Preferences &preferences, application::SessionPort &runtime)
    : preferences_(preferences), runtime_(runtime), monitor_(runtime), applied_design_(preferences.design()),
      pending_design_(preferences.design() == QStringLiteral("classic") ? 1 : 0) {
    design_initialized_ = preferences.available();
    connect(&preferences_, &Preferences::changed, this, &ManagerModel::changed);
    connect(&preferences_, &Preferences::errorOccurred, this, [this](const QString &message) {
        design_restart_pending_ = false;
        reset_pending_ = false;
        emit errorOccurred(message);
    });
    connect(&preferences_, &Preferences::changed, this, &ManagerModel::observe_design);
    connect(&preferences_, &Preferences::languageChanged, this, &ManagerModel::changed);
    connect(&monitor_, &RuntimeMonitor::changed, this, &ManagerModel::changed);
    connect(&displays_, &DisplayCatalog::changed, this, &ManagerModel::displaysChanged);
}

void ManagerModel::observe_design() {

    if (!design_initialized_) {
        applied_design_ = preferences_.design();
        pending_design_ = applied_design_ == QStringLiteral("classic") ? 1 : 0;
        design_initialized_ = true;
        emit changed();
    }
    if (reset_pending_ && preferences_.design() == QStringLiteral("fluent")) {
        reset_pending_ = false;
        pending_design_ = preferences_.design() == QStringLiteral("classic") ? 1 : 0;
        emit changed();
    }
    const auto requested = pending_design_ == 1 ? QStringLiteral("classic") : QStringLiteral("fluent");
    if (design_restart_pending_ && preferences_.design() == requested) {
        design_restart_pending_ = false;
        emit restartRequested();
    }
}

int ManagerModel::languageIndex() const {
    return index_of(languages, preferences_.language());
}
int ManagerModel::themeIndex() const {
    return index_of(themes, preferences_.theme());
}

int ManagerModel::osdPositionIndex() const {
    const auto value = preferences_.osd_position().toStdString();
    const auto &positions = application::osd_positions;
    const auto found = std::find(positions.begin(), positions.end(), value);
    return found == positions.end() ? 0 : static_cast<int>(found - positions.begin());
}
bool ManagerModel::trayAutostart() const {
    return preferences_.tray_autostart();
}

int ManagerModel::fnSAction() const {
    return preferences_.fn_s_action();
}

int ManagerModel::vendorKeyAction() const {
    return preferences_.vendor_key_action();
}
void ManagerModel::setVendorKeyAction(int action) {
    preference_error_ = !preferences_.set_vendor_key_action(action);
    emit changed();
}

QString ManagerModel::osdScreen() const {
    return preferences_.osd_screen();
}
int ManagerModel::designIndex() const {
    return pending_design_;
}
bool ManagerModel::autoUpdates() const {
    return preferences_.auto_updates();
}
QVariantList ManagerModel::displays() const {
    return displays_.screens();
}
void ManagerModel::setOsdScreen(const QString &id) {
    preference_error_ = !preferences_.set_osd_screen(id);
    emit changed();
}
void ManagerModel::setDesign(int index) {
    if (index < 0 || index > 1) {
        return;
    }
    pending_design_ = index;
    emit changed();
}

void ManagerModel::resetSettings() {
    preference_error_ = false;
    reset_pending_ = true;
    if (!preferences_.reset_defaults()) {
        reset_pending_ = false;
        emit errorOccurred(tr("Could not reset preferences."));
        return;
    }
    emit changed();
}

bool ManagerModel::designPending() const {
    return (pending_design_ == 1 ? QStringLiteral("classic") : QStringLiteral("fluent")) != applied_design_;
}
void ManagerModel::applyDesign() {
    if (!designPending() || design_restart_pending_) {
        return;
    }
    const auto requested = pending_design_ == 1 ? QStringLiteral("classic") : QStringLiteral("fluent");
    if (preferences_.available() && preferences_.design() == requested) {
        emit restartRequested();
        return;
    }
    design_restart_pending_ = true;
    preference_error_ =
        !preferences_.set_design(pending_design_ == 1 ? QStringLiteral("classic") : QStringLiteral("fluent"));
    emit changed();
    if (preference_error_)
        design_restart_pending_ = false;
}
bool ManagerModel::roundedCorners() const {
    return preferences_.rounded_corners();
}
qreal ManagerModel::cornerRadius() const {
    return roundedCorners() ? 8.0 : 0.0;
}
void ManagerModel::setRoundedCorners(bool enabled) {
    preference_error_ = !preferences_.set_rounded_corners(enabled);
    emit changed();
}
void ManagerModel::setAutoUpdates(bool enabled) {
    preference_error_ = !preferences_.set_auto_updates(enabled);
    emit changed();
}
void ManagerModel::setOsdPosition(int index) {
    if (index < 0 || index >= static_cast<int>(application::osd_positions.size())) {
        return;
    }
    const auto position = application::osd_positions.at(static_cast<std::size_t>(index));
    preference_error_ = !preferences_.set_osd_position(
        QString::fromUtf8(position.data(), static_cast<qsizetype>(position.size())));
    emit changed();
}
void ManagerModel::setTrayAutostart(bool enabled) {
    preference_error_ = !preferences_.set_tray_autostart(enabled);
    emit changed();
}

void ManagerModel::setFnSAction(int action) {
    preference_error_ = !preferences_.set_fn_s_action(action);
    emit changed();
}

QString ManagerModel::runtimeStatus() const {
    if (preference_error_) {
        return tr("Could not save preferences.");
    }
    if (busy_) {
        return tr("Waiting for background components…");
    }
    return monitor_.status_text();
}

QString ManagerModel::daemonDetails() const {
    const auto &reply_ = monitor_.reply();
    if (!reply_.connected) {
        return tr("Open the tray or retry after starting the session runtime.");
    }
    if (!reply_.daemon.connected) {
        return tr("The core did not respond. Check the background service or restart the components.");
    }
    return tr("Session PID: %1 · Daemon PID: %2 · Version: %3 · Build: %4")
        .arg(reply_.process_id)
        .arg(reply_.daemon.process_id)
        .arg(QString::fromStdString(reply_.daemon.version), QString::fromStdString(reply_.daemon.build));
}

void ManagerModel::setLanguage(int index) {
    if (index < 0 || index >= static_cast<int>(languages.size())) {
        return;
    }
    preference_error_ = !preferences_.set_language(languages.at(static_cast<std::size_t>(index)));
    emit changed();
}

void ManagerModel::setTheme(int index) {
    if (index < 0 || index >= static_cast<int>(themes.size())) {
        return;
    }
    preference_error_ = !preferences_.set_theme(themes.at(static_cast<std::size_t>(index)));
    emit changed();
}

void ManagerModel::request(application::SessionCommand command) {
    if (busy())
        return;
    busy_ = true;
    emit changed();
    const QPointer<ManagerModel> receiver(this);
    const bool started = runtime_.request(command, [receiver](const application::SessionReply &reply) {
        if (receiver)
            receiver->finishRequest(reply);
    });
    if (!started) {
        busy_ = false;
        emit changed();
        emit errorOccurred(tr("Session runtime unavailable"));
    }
}

void ManagerModel::finishRequest(const application::SessionReply &reply) {
    monitor_.accept(reply);
    busy_ = false;
    emit changed();
    if (!reply.connected || !reply.daemon.connected)
        emit errorOccurred(monitor_.status_text());
}

void ManagerModel::diagnostic() {
    if (!preferences_.set_osd_screen(QStringLiteral("auto"))) {
        preference_error_ = true;
        emit changed();
        return;
    }
    request(application::SessionCommand::Diagnostic);
}
void ManagerModel::refresh() {
    monitor_.refresh();
}
void ManagerModel::openTray() {
    request(application::SessionCommand::OpenTray);
}
} // namespace manager::presentation
