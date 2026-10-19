#include "apps/device/vendor_events.hpp"

#include <QJsonArray>
#include <QTextStream>

#if defined(Q_OS_WIN)
#include "adapters/windows/shortcuts/vendor_events.hpp"
#endif

namespace manager::device {

QJsonObject vendor_events(unsigned int timeout_ms) {
#if defined(Q_OS_WIN)
    const auto report = adapters::windows_shortcuts::probe_vendor_events(timeout_ms, [] {
        QTextStream(stderr) << "HID_EVENT20 subscription ready: press the physical Mi button now."
                            << Qt::endl;
    });
    QJsonArray events;
    for (const auto &event : report.events) {
        QJsonArray bytes;
        for (const auto byte : event.detail)
            bytes.append(static_cast<int>(byte));
        events.append(QJsonObject{{QStringLiteral("elapsedMs"), static_cast<qint64>(event.elapsed_ms)},
                                  {QStringLiteral("eventDetail"), bytes}});
    }
    return {{QStringLiteral("source"), QStringLiteral("ROOT/WMI HID_EVENT20")},
            {QStringLiteral("status"), static_cast<int>(report.status)},
            {QStringLiteral("subscribed"), report.subscribed},
            {QStringLiteral("eventLimitReached"), report.event_limit_reached},
            {QStringLiteral("events"), events},
            {QStringLiteral("detail"), QString::fromStdString(report.detail)}};
#else
    Q_UNUSED(timeout_ms)
    return {{QStringLiteral("status"), QStringLiteral("NotImplemented")},
            {QStringLiteral("subscribed"), false}};
#endif
}

} // namespace manager::device
