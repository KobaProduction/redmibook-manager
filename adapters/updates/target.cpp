#include "adapters/updates/target.hpp"
#include <QSysInfo>
namespace manager::adapters {
QString update_target() {
    return QStringLiteral(MANAGER_TARGET_OS "-") + QSysInfo::currentCpuArchitecture();
}
} // namespace manager::adapters
