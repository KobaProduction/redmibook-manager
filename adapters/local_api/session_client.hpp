#pragma once
#include "application/runtime/session_port.hpp"
#include <QLocalSocket>
#include <QObject>
#include <QString>
#include <QTimer>

namespace manager::adapters {
[[nodiscard]] QString session_endpoint();
[[nodiscard]] QString preferences_endpoint();
[[nodiscard]] QString preferences_directory();
void configure_gui_identity();
bool launch_component(const QString &name, const QStringList &arguments = {});
bool ensure_session();

class SessionClient final : public QObject, public application::SessionPort {
    Q_OBJECT
  public:
    explicit SessionClient(QObject *parent = nullptr);
    bool request(application::SessionCommand command,
                 std::function<void(application::SessionReply)> completion) override;

  private:
    QLocalSocket socket_;
    QTimer timeout_;
    QByteArray buffer_;
    QByteArray request_;
    bool active_{false};
    std::function<void(application::SessionReply)> completion_;
    void complete(const QString &status);
    void receive();
};
} // namespace manager::adapters
