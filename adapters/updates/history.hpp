#pragma once
#include <QString>
namespace manager::adapters {
class UpdateHistory final {
  public:
    explicit UpdateHistory(QString directory);
    bool claim_daily_check();

  private:
    QString directory_;
};
} // namespace manager::adapters
