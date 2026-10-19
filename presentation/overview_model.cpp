#include "presentation/overview_model.hpp"

namespace manager::presentation {
OverviewModel::OverviewModel(const application::Overview &overview, QObject *parent)
    : QObject(parent), greeting_(QString::fromStdString(overview.greeting)),
      platform_name_(overview.platform == domain::HostPlatform::Windows ? QStringLiteral("Windows")
                                                                        : QStringLiteral("Linux")) {}

QString OverviewModel::greeting() const {
    return greeting_;
}

QString OverviewModel::platformName() const {
    return platform_name_;
}
} // namespace manager::presentation
