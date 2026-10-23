#pragma once
#include "adapters/storage/ui_preferences/store.hpp"
#include <QLocalServer>
#include <QLocalSocket>
#include <QPointer>
#include <QSet>
#include <QTimer>

namespace manager::adapters {
class PreferenceServer final : public QObject {
  public:
    PreferenceServer(UiPreferenceStore &store, QString endpoint);
    ~PreferenceServer() override;
    bool listen();
    void broadcast();

  private:
    UiPreferenceStore &store_;
    QString endpoint_;
    QLocalServer server_;
    QSet<QLocalSocket *> peers_;
    void accept();
    void handle(QLocalSocket *socket);
    bool handle_frame(QLocalSocket *socket, const QJsonObject &object);
    bool apply(const QJsonObject &object);
    void send(QLocalSocket *socket, qint64 id = 0, bool ok = true);
};
class PreferenceClient final : public QObject, public application::PreferencePort {
  public:
    explicit PreferenceClient(QString endpoint);
    ~PreferenceClient() override;
    std::optional<application::UiPreferences> read() const override;
    application::PreferenceConnection connection() const override {
        return connection_;
    }
    void reconnect() override;
    void on_connection_failure(std::function<void()> handler) {
        recovery_ = std::move(handler);
    }
    void observe_connection(const std::function<void()> &changed) override {
        connection_changed_ = changed;
    }
    bool update(std::string_view key, std::string_view value) override;
    bool reset_defaults() override;
    bool save_tray_presets(const std::vector<application::presets::Preset> &presets) override;
    void observe(std::function<void()> changed) override {
        changed_ = std::move(changed);
    }
    void observe_error(const std::function<void()> &error) override {
        error_ = error;
    }

  private:
    QString endpoint_;
    QLocalSocket socket_;
    QTimer reconnect_;
    QTimer deadline_;
    QSet<qint64> pending_;
    qint64 next_id_{1};
    bool compatible_{false};
    application::PreferenceConnection connection_{application::PreferenceConnection::Connecting};
    std::function<void()> connection_changed_;
    void set_connection(application::PreferenceConnection state);
    int reconnect_delay_{250};
    std::optional<application::UiPreferences> cache_;
    std::function<void()> changed_;
    std::function<void()> error_;
    std::function<void()> recovery_;
    void connect_server();
    void schedule_reconnect();
    void receive();
    bool receive_frame(const QJsonObject &message);
    bool accept_snapshot(const QJsonObject &message);
    bool request(QJsonObject message);
    void fail();
};
} // namespace manager::adapters
