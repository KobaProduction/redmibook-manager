#pragma once
#include <QLocalServer>
#include <QLockFile>
#include <QObject>
#include <QQmlApplicationEngine>
#include <cstdint>
#include <memory>

namespace manager::desktop {
enum class ManagerActivation : std::uint8_t { KeepCurrent, Overview };

class ManagerInstance final : public QObject {
    Q_OBJECT
  public:
    static bool activate_existing(ManagerActivation activation);
    bool acquire(QQmlApplicationEngine &engine,
                 ManagerActivation activation = ManagerActivation::KeepCurrent);

  private:
    QLocalServer server_;
    std::unique_ptr<QLockFile> lock_;
};
} // namespace manager::desktop
