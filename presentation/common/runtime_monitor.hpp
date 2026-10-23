#pragma once
#include "application/runtime/connection_health.hpp"
#include <QObject>
#include <QString>
#include <QTimer>

namespace manager::presentation {
class RuntimeMonitor final : public QObject {
    Q_OBJECT
  public:
    explicit RuntimeMonitor(application::SessionPort &port);
    void refresh();
    void set_foreground(bool foreground);
    void accept(const application::SessionReply &reply);
    [[nodiscard]] const application::SessionReply &reply() const {
        return reply_;
    }
    [[nodiscard]] QString status_text() const;
    [[nodiscard]] bool pending() const {
        return pending_;
    }
  signals:
    void changed();

  private:
    application::SessionPort &port_;
    application::SessionReply reply_;
    QTimer heartbeat_;
    bool pending_{false};
};
} // namespace manager::presentation
