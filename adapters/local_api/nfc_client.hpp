#pragma once
#include "application/nfc/port.hpp"
#include <QJsonObject>
#include <QLocalSocket>
#include <QObject>
#include <QTimer>

namespace manager::adapters {
class NfcClient final : public QObject, public application::nfc::ClientPort {
    Q_OBJECT
  public:
    explicit NfcClient(QString endpoint, QObject *parent = nullptr);
    ~NfcClient() override;
    void observe(std::function<void(const application::nfc::Snapshot &)> observer) override;
    void refresh() override;
    bool write(application::nfc::WriteRequest request,
               std::function<void(application::nfc::Status)> completion) override;
    void start();

  private:
    QString endpoint_;
    QLocalSocket socket_;
    QTimer deadline_;
    application::nfc::Snapshot state_;
    std::function<void(const application::nfc::Snapshot &)> observer_;
    std::function<void(application::nfc::Status)> completion_;
    quint64 token_{};
    void receive();
    void accept(const QJsonObject &object);
    void unavailable(application::nfc::Status status);
    void finish(application::nfc::Status status);
    void publish();
};
} // namespace manager::adapters
