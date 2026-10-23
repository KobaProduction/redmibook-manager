#include "adapters/platform/battery.hpp"
#include "adapters/windows/hosting/handle.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <poclass.h>
#include <setupapi.h>
#include <vector>
#include <windows.h>
#include <winioctl.h>

namespace manager::adapters {
namespace {
using application::battery::Telemetry;
using application::battery::TelemetryStatus;
constexpr GUID battery_interface{
    0x72631e54, 0x78a4, 0x11d0, {0xbc, 0xf7, 0x00, 0xaa, 0x00, 0xb7, 0xb3, 0x2a}};
struct DeviceList {
    HDEVINFO value{
        SetupDiGetClassDevsW(&battery_interface, nullptr, nullptr, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE)};
    ~DeviceList() {
        if (value != INVALID_HANDLE_VALUE)
            SetupDiDestroyDeviceInfoList(value);
    }
};
template <class Input, class Output> bool query(HANDLE device, DWORD code, Input &input, Output &output) {
    windows::Handle event(CreateEventW(nullptr, TRUE, FALSE, nullptr));
    if (!event.valid())
        return false;
    OVERLAPPED operation{};
    operation.hEvent = event.get();
    DWORD received{};
    if (DeviceIoControl(device, code, &input, sizeof(input), &output, sizeof(output), &received, &operation))
        return received == sizeof(output);
    if (GetLastError() != ERROR_IO_PENDING)
        return false;
    if (WaitForSingleObject(event.get(), 2000) != WAIT_OBJECT_0) {
        CancelIoEx(device, &operation);
        // Keep stack buffers alive until the cancelled operation actually completes.
        GetOverlappedResult(device, &operation, &received, TRUE);
        return false;
    }
    return GetOverlappedResult(device, &operation, &received, FALSE) && received == sizeof(output);
}
void capacities(Telemetry &result, const BATTERY_INFORMATION &info, const BATTERY_STATUS &state) {
    if (state.Capacity != BATTERY_UNKNOWN_CAPACITY && info.FullChargedCapacity != BATTERY_UNKNOWN_CAPACITY &&
        info.FullChargedCapacity > 0)
        result.percent = std::clamp(
            static_cast<int>(std::lround(100.0 * state.Capacity / info.FullChargedCapacity)), 0, 100);
    if (info.Capabilities & BATTERY_CAPACITY_RELATIVE)
        return;
    if (state.Capacity != BATTERY_UNKNOWN_CAPACITY)
        result.remaining_wh = state.Capacity / 1000.0;
    if (info.FullChargedCapacity != BATTERY_UNKNOWN_CAPACITY && info.FullChargedCapacity > 0)
        result.full_wh = info.FullChargedCapacity / 1000.0;
    if (info.DesignedCapacity != BATTERY_UNKNOWN_CAPACITY && info.DesignedCapacity > 0)
        result.design_wh = info.DesignedCapacity / 1000.0;
    if (state.Rate != static_cast<LONG>(BATTERY_UNKNOWN_RATE))
        result.rate_w = state.Rate / 1000.0;
}
Telemetry measured(const BATTERY_INFORMATION &info, const BATTERY_STATUS &state) {
    Telemetry result;
    result.status = TelemetryStatus::Available;
    result.online = (state.PowerState & BATTERY_POWER_ON_LINE) != 0;
    result.charging = (state.PowerState & BATTERY_CHARGING) != 0;
    result.discharging = (state.PowerState & BATTERY_DISCHARGING) != 0;
    if (state.Voltage != BATTERY_UNKNOWN_VOLTAGE)
        result.voltage = state.Voltage / 1000.0;
    capacities(result, info, state);
    return result;
}
Telemetry read_device(const wchar_t *path) {
    windows::Handle device(CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                                       OPEN_EXISTING, FILE_FLAG_OVERLAPPED, nullptr));
    Telemetry result;
    result.status = TelemetryStatus::Failed;
    ULONG timeout{}, tag{};
    if (!device.valid() || !query(device.get(), IOCTL_BATTERY_QUERY_TAG, timeout, tag) || tag == 0)
        return result;
    BATTERY_QUERY_INFORMATION request{tag, BatteryInformation, 0};
    BATTERY_INFORMATION info{};
    BATTERY_WAIT_STATUS wait{tag, 0, 0, 0, 0};
    BATTERY_STATUS state{};
    if (!query(device.get(), IOCTL_BATTERY_QUERY_INFORMATION, request, info) ||
        !(info.Capabilities & BATTERY_SYSTEM_BATTERY) ||
        !query(device.get(), IOCTL_BATTERY_QUERY_STATUS, wait, state))
        return result;
    return measured(info, state);
}
std::vector<wchar_t> device_path(HDEVINFO list, SP_DEVICE_INTERFACE_DATA &entry) {
    DWORD required{};
    SetupDiGetDeviceInterfaceDetailW(list, &entry, nullptr, 0, &required, nullptr);
    if (required < sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W) || required > 65536)
        return {};
    std::vector<wchar_t> storage((required + sizeof(wchar_t) - 1) / sizeof(wchar_t));
    auto *detail = reinterpret_cast<SP_DEVICE_INTERFACE_DETAIL_DATA_W *>(storage.data());
    detail->cbSize = sizeof(*detail);
    if (!SetupDiGetDeviceInterfaceDetailW(list, &entry, detail, required, nullptr, nullptr))
        return {};
    return std::vector<wchar_t>(detail->DevicePath, detail->DevicePath + wcslen(detail->DevicePath) + 1);
}
bool next_battery_interface(HDEVINFO list, DWORD index, SP_DEVICE_INTERFACE_DATA &entry) {
    entry = {};
    entry.cbSize = sizeof(entry);
    return SetupDiEnumDeviceInterfaces(list, nullptr, &battery_interface, index, &entry);
}
std::vector<wchar_t> select_battery(HDEVINFO list) {
    std::vector<wchar_t> selected;
    for (DWORD index = 0; index < 32; ++index) {
        SP_DEVICE_INTERFACE_DATA entry{};
        if (!next_battery_interface(list, index, entry))
            break;
        auto path = device_path(list, entry);
        if (path.empty() || read_device(path.data()).status != TelemetryStatus::Available)
            continue;
        if (!selected.empty())
            return {};
        selected = std::move(path);
    }
    return selected;
}

class Battery final : public application::battery::TelemetryPort {
    std::unique_ptr<windows::Handle> device_;
    BATTERY_INFORMATION information_{};
    ULONG tag_{};
    std::chrono::steady_clock::time_point discovered_{};
    std::vector<wchar_t> discover() {
        DeviceList list;
        return list.value == INVALID_HANDLE_VALUE ? std::vector<wchar_t>{} : select_battery(list.value);
    }
    bool open() {
        device_.reset();
        const auto path = discover();
        if (path.empty())
            return false;
        device_ = std::make_unique<windows::Handle>(
            CreateFileW(path.data(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING,
                        FILE_FLAG_OVERLAPPED, nullptr));
        ULONG timeout{};
        if (!device_->valid() || !query(device_->get(), IOCTL_BATTERY_QUERY_TAG, timeout, tag_) || tag_ == 0)
            return false;
        BATTERY_QUERY_INFORMATION request{tag_, BatteryInformation, 0};
        return query(device_->get(), IOCTL_BATTERY_QUERY_INFORMATION, request, information_) &&
               (information_.Capabilities & BATTERY_SYSTEM_BATTERY);
    }
    Telemetry read() override {
        const auto now = std::chrono::steady_clock::now();
        if (!device_ || now - discovered_ >= std::chrono::seconds(60)) {
            if (!open()) {
                device_.reset();
                return {};
            }
            discovered_ = now;
        }
        BATTERY_WAIT_STATUS wait{tag_, 0, 0, 0, 0};
        BATTERY_STATUS state{};
        if (!query(device_->get(), IOCTL_BATTERY_QUERY_STATUS, wait, state)) {
            device_.reset();
            Telemetry failed;
            failed.status = TelemetryStatus::Failed;
            return failed;
        }
        return measured(information_, state);
    }
};
} // namespace
std::shared_ptr<application::battery::TelemetryPort> make_battery_telemetry() {
    return std::make_shared<Battery>();
}
} // namespace manager::adapters
