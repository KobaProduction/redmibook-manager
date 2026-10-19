#pragma once
#include <QApplication>
#include <QLocalServer>
#include <QLockFile>

namespace manager::tray {
bool close_existing();
bool enable_close_endpoint(QLocalServer &server);
class Instance final {
  public:
    explicit Instance(const QString &directory);
    bool acquire();

  private:
    QLockFile handover_;
    QLockFile instance_;
    QLocalServer endpoint_;
};
int dashboard_smoke(QApplication &app, const QString &screenshot);
} // namespace manager::tray
