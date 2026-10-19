#pragma once
#include "presentation/common/preferences.hpp"
#include "presentation/manager_model.hpp"
#include <QQmlApplicationEngine>
namespace manager::desktop {
inline constexpr int restart_manager_exit = 42;
void wire_manager_appearance(presentation::Preferences &preferences, presentation::ManagerModel &model,
                             QQmlApplicationEngine &engine);
} // namespace manager::desktop
