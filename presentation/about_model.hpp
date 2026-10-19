#pragma once
#include "application/device_profile/profile_port.hpp"
#include "presentation/common/preferences.hpp"
#include <QObject>
#include <QtQml/qqmlregistration.h>
namespace manager::presentation {
class AboutModel final : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Created by application composition")
    Q_PROPERTY(bool hasTranslation READ hasTranslation NOTIFY textsChanged)
    Q_PROPERTY(QString firmwareChip READ firmwareChip CONSTANT)
    Q_PROPERTY(QString mitTranslation READ mitTranslation NOTIFY textsChanged)
    Q_PROPERTY(QString model READ model CONSTANT)
    Q_PROPERTY(QString biosVendor READ biosVendor CONSTANT)
    Q_PROPERTY(QString biosVersion READ biosVersion CONSTANT)
    Q_PROPERTY(QString controller READ controller CONSTANT)
    Q_PROPERTY(bool demo READ demo CONSTANT)
  public:
    AboutModel(const application::DeviceProfilePort &source, Preferences &preferences)
        : profile_(source.read()) {
        connect(&preferences, &Preferences::languageChanged, this, &AboutModel::textsChanged);
    }
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
    QString mitTranslation() const;
    Q_INVOKABLE QString licenseText(int index, bool original) const;
  signals:
    void textsChanged();

  private:
    application::DeviceProfile profile_;
};
} // namespace manager::presentation
