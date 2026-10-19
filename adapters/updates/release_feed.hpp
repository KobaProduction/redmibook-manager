#pragma once
#include "application/updates/release_policy.hpp"
#include "application/updates/update_port.hpp"
#include <QJsonArray>
namespace manager::adapters {
application::UpdateResult decode_releases(const QJsonArray &releases, const QString &target);
QString release_asset(const QJsonObject &object, const application::ReleaseVersion &version,
                      const QString &target);
} // namespace manager::adapters
