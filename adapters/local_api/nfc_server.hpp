#pragma once
#include "application/nfc/port.hpp"
#include <QFutureWatcher>
#include <QJsonObject>
#include <QLocalServer>
#include <QLocalSocket>
#include <QObject>
#include <QPointer>
#include <QThreadPool>
#include <QTimer>
#include <mutex>

namespace manager::adapters {
class NfcServer final : public QObject {
    Q_OBJECT
  public:
    NfcServer(std::shared_ptr<application::nfc::TagPort> tag, QThreadPool &pool, QString endpoint,
              QObject *parent = nullptr);
    ~NfcServer() override;
    bool listen();
    void refresh();

  private:
    struct Delivery {
        std::mutex mutex;
        NfcServer *target{};
        bool queued{};
    };
    std::shared_ptr<application::nfc::TagPort> tag_;
    QThreadPool &pool_;
    QString endpoint_;
    QLocalServer server_;
    std::vector<QPointer<QLocalSocket>> clients_;
    QFutureWatcher<application::nfc::Result> pending_;
    QTimer deadline_;
    std::shared_ptr<Delivery> delivery_{std::make_shared<Delivery>()};
    std::unique_ptr<application::nfc::Subscription> watch_;
    application::nfc::Snapshot state_;
    QPointer<QLocalSocket> writer_;
    QString token_;
    bool busy_{};
    bool again_{};
    bool expired_{};
    void accept();
    void receive(QLocalSocket *socket);
    void request(QLocalSocket *socket, const QJsonObject &object);
    void write(QLocalSocket *socket, const QJsonObject &object);
    void complete();
    void publish(const application::nfc::Snapshot &state);
    void result(QLocalSocket *socket, const QString &token, application::nfc::Status status);
    void watch();
    void begin();
};
} // namespace manager::adapters
