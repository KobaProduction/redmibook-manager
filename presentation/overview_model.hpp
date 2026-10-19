#pragma once

#include "application/overview/read_overview.hpp"

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtQml/qqmlregistration.h>

namespace manager::presentation {
class OverviewModel final : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Created by the desktop composition root")
    Q_PROPERTY(QString greeting READ greeting CONSTANT FINAL)
    Q_PROPERTY(QString platformName READ platformName CONSTANT FINAL)

  public:
    explicit OverviewModel(const application::Overview &overview, QObject *parent = nullptr);
    [[nodiscard]] QString greeting() const;
    [[nodiscard]] QString platformName() const;

  private:
    QString greeting_;
    QString platform_name_;
};
} // namespace manager::presentation
