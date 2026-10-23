#include "apps/device/qualification.hpp"
#include "adapters/device_profile/native_profile.hpp"
#include "adapters/windows/battery/charge_control.hpp"
#include "adapters/windows/hosting/handle.hpp"
#include "adapters/windows/performance/performance_control.hpp"
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>
#include <stdexcept>

namespace manager::device {
namespace {
namespace fw = adapters::windows_firmware;
namespace battery = application::battery;
namespace perf = application::performance;
class DeviceLock {
  public:
    DeviceLock()
        : mutex_(CreateMutexW(nullptr, FALSE, L"Global\\KobaProduction.Redmibook.FirmwareQualification")) {
        acquired_ = mutex_.valid() && WaitForSingleObject(mutex_.get(), 0) == WAIT_OBJECT_0;
    }
    ~DeviceLock() {
        if (acquired_) {
            ReleaseMutex(mutex_.get());
        }
    }
    bool acquired() const {
        return acquired_;
    }

  private:
    adapters::windows::Handle mutex_;
    bool acquired_{false};
};
class Qualification {
  public:
    Qualification(const fw::FirmwareIdentity &identity, QString path)
        : path_(std::move(path)), battery_(transport_, identity), performance_(transport_, identity) {}
    QJsonObject run();

  private:
    QString path_;
    fw::NativeWmiTransport transport_;
    adapters::windows_battery::ChargeControl battery_;
    adapters::windows_performance::PerformanceControl performance_;
    battery::ChargeLimit original_charge_{battery::ChargeLimit::Full};
    perf::Profile original_profile_{perf::Profile::Balanced};
    QJsonObject report_;
    QJsonArray steps_;
    bool unsettled_write_{false};
    void save();
    void snapshot();
    bool apply(fw::MifsCommand command, bool charge, int expected);
    void trials();
    bool restore();
};
void Qualification::save() {
    report_.insert(QStringLiteral("steps"), steps_);
    QSaveFile file(path_);
    const auto bytes = QJsonDocument(report_).toJson();
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit()) {
        throw std::runtime_error("Cannot persist qualification journal");
    }
}
void Qualification::snapshot() {
    if (QFileInfo::exists(path_)) {
        throw std::runtime_error("Qualification journal already exists; preserve it for recovery");
    }
    const auto charge = battery_.read();
    const auto profile = performance_.read();
    if (charge.status != battery::Status::Available || !charge.limit ||
        profile.status != perf::Status::Available || !profile.profile) {
        throw std::runtime_error("Both original firmware settings must be readable before any SET");
    }
    original_charge_ = *charge.limit;
    original_profile_ = *profile.profile;
    report_ = {{QStringLiteral("bios"), QStringLiteral("RMAMT6B0P0A0A")},
               {QStringLiteral("board"), QStringLiteral("TM2309")},
               {QStringLiteral("originalCharge"), static_cast<int>(original_charge_)},
               {QStringLiteral("originalProfile"), static_cast<int>(original_profile_)},
               {QStringLiteral("phase"), QStringLiteral("baseline-saved")}};
    save();
}
bool Qualification::apply(fw::MifsCommand command, bool charge, int expected) {
    const auto reply = fw::validate_reply(command, transport_.exchange(command));
    unsettled_write_ = unsettled_write_ || reply.status == fw::TransportStatus::OutcomeUnknown;
    int observed = -1;
    if (charge) {
        const auto value = battery_.read();
        if (value.status == battery::Status::Available && value.limit) {
            observed = static_cast<int>(*value.limit);
        }
    } else {
        const auto value = performance_.read();
        if (value.status == perf::Status::Available && value.profile) {
            observed = static_cast<int>(*value.profile);
        }
    }
    const bool confirmed = reply.status == fw::TransportStatus::Success && observed == expected;
    steps_.append(QJsonObject{
        {QStringLiteral("kind"), charge ? QStringLiteral("charge") : QStringLiteral("performance")},
        {QStringLiteral("expected"), expected},
        {QStringLiteral("observed"), observed},
        {QStringLiteral("accepted"), reply.status == fw::TransportStatus::Success},
        {QStringLiteral("detail"), QString::fromStdString(reply.detail)},
        {QStringLiteral("confirmed"), confirmed}});
    return confirmed;
}
void Qualification::trials() {
    report_.insert(QStringLiteral("phase"), QStringLiteral("trial-writes-in-progress"));
    save();
    if (!apply(fw::MifsCommand::Protect80, true, 80) || !apply(fw::MifsCommand::FullCharge, true, 100)) {
        throw std::runtime_error("Charge policy SET/readback did not match");
    }
    save();
    if (!apply(fw::MifsCommand::Balanced, false, 1) || !apply(fw::MifsCommand::Quiet, false, 2) ||
        !apply(fw::MifsCommand::Turbo, false, 3)) {
        throw std::runtime_error("Performance SET/readback did not match");
    }
}
bool Qualification::restore() {
    const auto charge = original_charge_ == battery::ChargeLimit::Protection80 ? fw::MifsCommand::Protect80
                                                                               : fw::MifsCommand::FullCharge;
    const auto profile = static_cast<fw::MifsCommand>(static_cast<int>(fw::MifsCommand::Balanced) +
                                                      static_cast<int>(original_profile_) - 1);
    const bool charge_restored = apply(charge, true, static_cast<int>(original_charge_));
    const bool profile_restored = apply(profile, false, static_cast<int>(original_profile_));
    report_.insert(QStringLiteral("chargeRestored"), charge_restored && !unsettled_write_);
    report_.insert(QStringLiteral("profileRestored"), profile_restored && !unsettled_write_);
    report_.insert(QStringLiteral("unsettledWrite"), unsettled_write_);
    return charge_restored && profile_restored && !unsettled_write_;
}
QJsonObject Qualification::run() {
    snapshot();
    bool trials_passed = false;
    try {
        trials();
        trials_passed = true;
    } catch (const std::exception &error) {
        report_.insert(QStringLiteral("error"), QString::fromUtf8(error.what()));
    }
    const bool restored = restore();
    report_.insert(QStringLiteral("confirmed"), trials_passed && restored);
    report_.insert(QStringLiteral("phase"),
                   restored ? QStringLiteral("restored") : QStringLiteral("restore-unconfirmed"));
    save();
    return report_;
}
} // namespace
QJsonObject qualify_firmware(const QString &journal_path) {
    const DeviceLock lock;
    if (!lock.acquired()) {
        return {{QStringLiteral("error"), QStringLiteral("another-firmware-qualification-is-active")}};
    }
    const auto identity = adapters::NativeDeviceProfile{}.read();
    if (identity.manufacturer != "XIAOMI" || identity.board_model != "TM2309" ||
        identity.bios_version != "RMAMT6B0P0A0A") {
        return {{QStringLiteral("error"), QStringLiteral("qualification-target-mismatch")}};
    }
    try {
        Qualification qualification({identity.manufacturer, identity.board_model, identity.bios_version},
                                    journal_path);
        return qualification.run();
    } catch (const std::exception &error) {
        return {{QStringLiteral("error"), QString::fromUtf8(error.what())},
                {QStringLiteral("confirmed"), false}};
    }
}
} // namespace manager::device
