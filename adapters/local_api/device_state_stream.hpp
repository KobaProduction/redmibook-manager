#pragma once
#include "application/overview/device_state.hpp"
#include <QLocalServer>
#include <QLocalSocket>
#include <QObject>
#include <QPointer>
#include <vector>

namespace manager::adapters {
class DeviceStateServer final : public QObject {
    Q_OBJECT
  public:
    bool listen();
    void publish(const application::overview::DeviceState &state);
  signals:
    void refreshRequested();

  private:
    QLocalServer server_;
    QByteArray current_;
    std::vector<QPointer<QLocalSocket>> clients_;
    void accept();
    void receive(QLocalSocket *socket);
};
class DeviceStateClient final : public QObject {
    Q_OBJECT
  public:
    explicit DeviceStateClient(QObject *parent = nullptr);
    void start();
    void refresh();
  signals:
    void stateChanged(const application::overview::DeviceState &state);
    void unavailable();

  private:
    QLocalSocket socket_;
    void receive();
};
} // namespace manager::adapters
