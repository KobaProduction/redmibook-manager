#pragma once
#include "application/updates/update_port.hpp"
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
  public:
    UpdateModel(application::UpdatePort &source, QString target);
    QString status() const;
    QDateTime checkedAt() const {
        return checked_at_;
    }
    bool busy() const {
        return busy_;
    }
    bool available() const {
        return checked_ && result_.state == application::UpdateState::Available;
    }
    QString downloadUrl() const;
    bool directDownload() const {
        return !result_.download_url.empty();
    }
    QString version() const {
        return QStringLiteral(MANAGER_APP_VERSION);
    }
    QString build() const {
        return QStringLiteral(MANAGER_BUILD_ID);
    }
    QString target() const {
        return target_;
    }
    Q_INVOKABLE void check();
  signals:
    void changed();

  private:
    application::UpdatePort &source_;
    QString target_;
    application::UpdateResult result_;
    bool checked_{false};
    bool busy_{false};
    QDateTime checked_at_;
};
} // namespace manager::presentation
