#pragma once
#include <QApplication>
int observe_preferences(QApplication &app, const QString &directory);
bool preferences_sync_contract();
