#pragma once

#include <QtCore/QString>

class QQmlApplicationEngine;

namespace manager::desktop {
void schedule_smoke_test(QQmlApplicationEngine &engine, const QString &screenshot);
}
