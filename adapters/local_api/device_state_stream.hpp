#pragma once
#include "application/overview/device_state.hpp"
#include <QElapsedTimer>
#include <QLocalServer>
#include <QLocalSocket>
#include <QObject>
#include <QPointer>
#include <QTimer>
#include <vector>

namespace manager::adapters {
class DeviceStateServer final : public QObject {
    Q_OBJECT
  public:
    ~DeviceStateServer() override;
    bool listen();
    void publish(const application::overview::DeviceState &state);
    void finishRefresh(bool success);
  signals:
    void refreshRequested();
    void batteryRefreshRequested();
    void batteryWatchChanged(int intervalMs);

  private:
    QLocalServer server_;
    QByteArray current_;
    std::vector<QPointer<QLocalSocket>> clients_;
    void accept();
    void receive(QLocalSocket *socket);
    bool request_refresh(QLocalSocket *socket, const QByteArray &line);
    bool request_battery_watch(QLocalSocket *socket, const QByteArray &line);
    void update_battery_watch();
    int requested_battery_interval() const;
    QElapsedTimer lease_clock_;
    QTimer lease_timer_;
    int battery_interval_{};
};
class DeviceStateClient final : public QObject {
    Q_OBJECT
  public:
    explicit DeviceStateClient(QObject *parent = nullptr);
    ~DeviceStateClient() override;
    void start();
    void refresh();
    void refreshBattery();
    void watchBattery(int intervalMs);
  signals:
    void stateChanged(const application::overview::DeviceState &state);
    void unavailable();
    void refreshStarted();
    void refreshFinished(application::overview::RefreshResult result);

  private:
    QLocalSocket socket_;
    QTimer refresh_deadline_;
    quint64 refresh_token_{};
    void receive();
    bool receive_refresh(const QByteArray &line);
    void finish_refresh(application::overview::RefreshResult result);
    void disconnected();
};
} // namespace manager::adapters
