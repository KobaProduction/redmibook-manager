#pragma once
#include "application/runtime/build_identity.hpp"
#include "application/updates/package_port.hpp"
#include <QDateTime>
#include <QObject>
#include <QtQml/qqmlregistration.h>

namespace manager::presentation {
class UpdateModel final : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Created by application composition")
    Q_PROPERTY(QString status READ status NOTIFY changed)
    Q_PROPERTY(QDateTime checkedAt READ checkedAt NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(bool available READ available NOTIFY changed)
    Q_PROPERTY(QString downloadUrl READ downloadUrl NOTIFY changed)
    Q_PROPERTY(QString version READ version CONSTANT)
    Q_PROPERTY(QString build READ build CONSTANT)
    Q_PROPERTY(QString target READ target CONSTANT)
    Q_PROPERTY(bool directDownload READ directDownload NOTIFY changed)
    Q_PROPERTY(bool canDownload READ canDownload NOTIFY changed)
    Q_PROPERTY(bool canInstall READ canInstall NOTIFY changed)
    Q_PROPERTY(bool downloading READ downloading NOTIFY changed)
    Q_PROPERTY(double progress READ progress NOTIFY changed)
  public:
    UpdateModel(application::UpdatePort &source, QString target,
                application::UpdatePackagePort *packages = nullptr);
    QString status() const;
    QDateTime checkedAt() const {
        return checked_at_;
    }
    bool busy() const {
        return busy_ || downloading() || transfer_ == Transfer::Launching;
    }
    bool available() const {
        return checked_ && result_.state == application::UpdateState::Available;
    }
    QString downloadUrl() const;
    bool directDownload() const {
        return !result_.download_url.empty();
    }
    bool canDownload() const;
    bool canInstall() const;
    bool downloading() const {
        return transfer_ == Transfer::Downloading;
    }
    double progress() const {
        return progress_;
    }
    QString version() const {
        return QString::fromLatin1(manager::application::build_identity::display_version);
    }
    QString build() const {
        return QString::fromLatin1(manager::application::build_identity::payload);
    }
    QString target() const {
        return target_;
    }
    Q_INVOKABLE void check();
    Q_INVOKABLE void download();
    Q_INVOKABLE void install();
  signals:
    void changed();

  private:
    enum class Transfer : std::uint8_t {
        Idle,
        Downloading,
        Ready,
        Launching,
        Started,
        DownloadFailed,
        InstallFailed,
        Cancelled
    };
    QString transferStatus() const;
    application::UpdatePort &source_;
    application::UpdatePackagePort *packages_;
    QString target_;
    application::UpdateResult result_;
    bool checked_{false};
    bool busy_{false};
    QDateTime checked_at_;
    Transfer transfer_{Transfer::Idle};
    double progress_{};
};
} // namespace manager::presentation
