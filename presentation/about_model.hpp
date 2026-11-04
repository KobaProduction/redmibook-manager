#pragma once
#include "application/device_profile/profile_port.hpp"
#include "presentation/common/preferences.hpp"
#include <QFutureWatcher>
#include <QObject>
#include <QTimer>
#include <QtQml/qqmlregistration.h>
#include <memory>
namespace manager::presentation {
class AboutModel final : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Created by application composition")
    Q_PROPERTY(bool hasTranslation READ hasTranslation NOTIFY textsChanged)
    Q_PROPERTY(QString firmwareChip READ firmwareChip NOTIFY profileChanged)
    Q_PROPERTY(QString mitTranslation READ mitTranslation NOTIFY textsChanged)
    Q_PROPERTY(QString model READ model NOTIFY profileChanged)
    Q_PROPERTY(QString biosVendor READ biosVendor NOTIFY profileChanged)
    Q_PROPERTY(QString biosVersion READ biosVersion NOTIFY profileChanged)
    Q_PROPERTY(QString controller READ controller NOTIFY profileChanged)
    Q_PROPERTY(QString manufacturer READ manufacturer NOTIFY profileChanged)
    Q_PROPERTY(QString biosDate READ biosDate NOTIFY profileChanged)
    Q_PROPERTY(QString boardModel READ boardModel NOTIFY profileChanged)
    Q_PROPERTY(QString profileStatus READ profileStatus NOTIFY textsChanged)
    Q_PROPERTY(bool demo READ demo NOTIFY profileChanged)
  public:
    AboutModel(application::DeviceProfile profile, Preferences &preferences) : profile_(std::move(profile)) {
        connect(&preferences, &Preferences::languageChanged, this, &AboutModel::textsChanged);
    }
    AboutModel(std::shared_ptr<const application::DeviceProfilePort> source, Preferences &preferences);
    bool hasTranslation() const {
        return mitTranslation().trimmed() != licenseText(0, true).trimmed();
    }
    QString firmwareChip() const {
        return QString::fromStdString(profile_.firmware_chip);
    }
    QString model() const {
        return QString::fromStdString(profile_.model);
    }
    QString biosVendor() const {
        return QString::fromStdString(profile_.bios_vendor);
    }
    QString biosVersion() const {
        return QString::fromStdString(profile_.bios_version);
    }
    QString controller() const {
        return QString::fromStdString(profile_.controller);
    }
    bool demo() const {
        return profile_.evidence == application::ProfileEvidence::Demo;
    }
    QString manufacturer() const {
        return QString::fromStdString(profile_.manufacturer);
    }
    QString biosDate() const {
        return QString::fromStdString(profile_.bios_date);
    }
    QString boardModel() const {
        return QString::fromStdString(profile_.board_model);
    }
    QString profileStatus() const;
    QString mitTranslation() const;
    Q_INVOKABLE QString licenseText(int index, bool original) const;
  signals:
    void textsChanged();
    void profileChanged();

  private:
    application::DeviceProfile profile_;
    QFutureWatcher<application::DeviceProfile> watcher_;
    QTimer deadline_;
    bool loading_{false};
    void completeProfile();
};
} // namespace manager::presentation
