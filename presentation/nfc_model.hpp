#pragma once
#include "application/nfc/port.hpp"
#include <QObject>
#include <QUrl>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

namespace manager::presentation {
class NfcModel final : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Created by the application")
    Q_PROPERTY(bool present READ present NOTIFY changed)
    Q_PROPERTY(bool writable READ writable NOTIFY changed)
    Q_PROPERTY(bool eventDriven READ eventDriven NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(QString name READ name NOTIFY changed)
    Q_PROPERTY(QString statusText READ statusText NOTIFY changed)
    Q_PROPERTY(QString fieldText READ fieldText NOTIFY changed)
    Q_PROPERTY(QString contentHex READ contentHex NOTIFY changed)
    Q_PROPERTY(int capacity READ capacity NOTIFY changed)
    Q_PROPERTY(int used READ used NOTIFY changed)
    Q_PROPERTY(qulonglong encounters READ encounters NOTIFY changed)
    Q_PROPERTY(QVariantList records READ records NOTIFY changed)
  public:
    explicit NfcModel(std::shared_ptr<application::nfc::ClientPort> client, QObject *parent = nullptr);
    ~NfcModel() override;
    bool present() const {
        return state_.present;
    }
    bool writable() const {
        return state_.writable && state_.status == application::nfc::Status::Ready && !busy_;
    }
    bool busy() const {
        return busy_;
    }
    bool eventDriven() const {
        return state_.event_driven;
    }
    QString name() const {
        return QString::fromStdString(state_.name);
    }
    QString statusText() const;
    QString fieldText() const;
    QString contentHex() const;
    int capacity() const {
        return static_cast<int>(state_.capacity);
    }
    int used() const {
        return static_cast<int>(state_.content.size());
    }
    qulonglong encounters() const {
        return state_.encounters;
    }
    QVariantList records() const;
    Q_INVOKABLE void refresh();
    Q_INVOKABLE void writeText(const QString &text);
    Q_INVOKABLE void writeUri(const QString &uri);
    Q_INVOKABLE void writeHex(const QString &hex);
    Q_INVOKABLE void exportTag(const QUrl &url);
    Q_INVOKABLE QString readBackup(const QUrl &url);
    void acceptState(const application::nfc::Snapshot &state);
  signals:
    void changed();
    void errorOccurred(const QString &message);

  protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

  private:
    std::shared_ptr<application::nfc::ClientPort> client_;
    application::nfc::Snapshot state_;
    bool busy_{};
    void write(application::nfc::Bytes content);
    void finish(application::nfc::Status status);
    static QString statusMessage(application::nfc::Status status);
};
} // namespace manager::presentation
