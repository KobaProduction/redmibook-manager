#pragma once
#include "application/feedback/device_feedback.hpp"
#include "application/runtime/read_status.hpp"
#include <QFutureWatcher>
#include <QLocalServer>
#include <QLocalSocket>
#include <QObject>
#include <QPointer>
#include <vector>

namespace manager::adapters {
class SessionServer final : public QObject {
    Q_OBJECT
  public:
    SessionServer(const application::ReadRuntimeStatus &read, application::DiagnosticFeedbackPort &feedback,
                  application::feedback::Presenter *device_feedback = nullptr);
    ~SessionServer() override;
    bool listen(const QString &endpoint = {});
  signals:
    void openTray();
    void restartRequested();

  private:
    const application::ReadRuntimeStatus &read_;
    application::DiagnosticFeedbackPort &feedback_;
    application::feedback::Presenter *device_feedback_;
    QLocalServer server_;
    QFutureWatcher<application::DiagnosticReply> watcher_;
    std::vector<QPointer<QLocalSocket>> pending_;
    bool probe_active_{false};
    bool show_feedback_{false};
    application::DiagnosticReply last_reply_;
    void accept();
    void handle(QLocalSocket *socket);
    void present_feedback(QLocalSocket *socket, const QByteArray &command);
    void stop_session(QLocalSocket *socket, bool restart);
    void completed();
    void respond(QLocalSocket *socket, const application::DiagnosticReply &reply);
    void probe(QLocalSocket *socket, bool feedback);
};
} // namespace manager::adapters
