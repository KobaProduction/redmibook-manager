#pragma once
#include "presentation/manager_model.hpp"
#include <QQmlApplicationEngine>

namespace manager::desktop {
void schedule_ui_contract(QQmlApplicationEngine &engine, presentation::ManagerModel &model,
                          const QString &screenshot);
void schedule_preset_preview(QQmlApplicationEngine &engine, presentation::ManagerModel &model,
                             const QString &screenshot);
} // namespace manager::desktop
