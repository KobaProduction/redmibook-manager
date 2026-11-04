#pragma once
#include <QFile>
#include <chrono>

namespace manager::desktop {
inline void trace_startup(const char *stage) {
    using Clock = std::chrono::steady_clock;
    static const auto started = Clock::now();
    const auto path = qEnvironmentVariable("REDMIBOOK_STARTUP_TRACE");
    if (path.isEmpty())
        return;
    QFile file(path);
    if (file.open(QIODevice::WriteOnly | QIODevice::Append)) {
        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - started);
        file.write(QByteArray::number(elapsed.count()) + " ms " + stage + '\n');
    }
}
} // namespace manager::desktop
