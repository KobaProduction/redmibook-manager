#pragma once
#include <QLocalServer>
#include <QLockFile>
#include <QObject>
#include <QQmlApplicationEngine>
#include <memory>

namespace manager::desktop {
class ManagerInstance final : public QObject {
    Q_OBJECT
  public:
    bool acquire(QQmlApplicationEngine &engine);

  private:
    QLocalServer server_;
    std::unique_ptr<QLockFile> lock_;
};
} // namespace manager::desktop
