#include "adapters/platform/session_controls.hpp"
#include <algorithm>
#include <cwchar>
#include <optional>
#include <string_view>
#include <utility>
#include <windows.h>

namespace manager::adapters {
namespace {
std::string utf8(const std::wstring &text) {
    const auto size = static_cast<int>(text.size());
    const int count = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), size, nullptr, 0, nullptr, nullptr);
    std::string value(static_cast<std::size_t>(count), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.c_str(), size, value.data(), count, nullptr, nullptr);
    return value;
}
DEVMODEW current_mode(const wchar_t *name) {
    DEVMODEW mode{};
    mode.dmSize = sizeof(mode);
    if (!EnumDisplaySettingsExW(name, ENUM_CURRENT_SETTINGS, &mode, 0)) {
        return {};
    }
    return mode;
}
bool same_geometry(const DEVMODEW &left, const DEVMODEW &right) {
    return left.dmPelsWidth == right.dmPelsWidth && left.dmPelsHeight == right.dmPelsHeight &&
           left.dmBitsPerPel == right.dmBitsPerPel &&
           left.dmDisplayOrientation == right.dmDisplayOrientation &&
           left.dmDisplayFlags == right.dmDisplayFlags && right.dmDisplayFrequency > 1;
}
std::vector<unsigned int> rates(const wchar_t *name, const DEVMODEW &current) {
    std::vector<unsigned int> result;
    DEVMODEW mode{};
    mode.dmSize = sizeof(mode);
    for (DWORD index = 0; EnumDisplaySettingsExW(name, index, &mode, 0); ++index) {
        if (same_geometry(current, mode)) {
            result.push_back(mode.dmDisplayFrequency);
        }
    }
    std::ranges::sort(result);
    const auto duplicates = std::ranges::unique(result);
    result.erase(duplicates.begin(), duplicates.end());
    return result;
}
bool internal_output(DISPLAYCONFIG_VIDEO_OUTPUT_TECHNOLOGY technology) {
    return technology == DISPLAYCONFIG_OUTPUT_TECHNOLOGY_INTERNAL ||
           technology == DISPLAYCONFIG_OUTPUT_TECHNOLOGY_LVDS ||
           technology == DISPLAYCONFIG_OUTPUT_TECHNOLOGY_DISPLAYPORT_EMBEDDED ||
           technology == DISPLAYCONFIG_OUTPUT_TECHNOLOGY_UDI_EMBEDDED;
}

std::vector<DISPLAYCONFIG_PATH_INFO> active_paths() {
    for (unsigned int attempt = 0; attempt < 3; ++attempt) {
        UINT32 path_count = 0;
        UINT32 mode_count = 0;
        if (GetDisplayConfigBufferSizes(QDC_ONLY_ACTIVE_PATHS, &path_count, &mode_count) != ERROR_SUCCESS ||
            path_count == 0 || path_count > 256 || mode_count == 0 || mode_count > 1024) {
            return {};
        }
        std::vector<DISPLAYCONFIG_PATH_INFO> paths(path_count);
        std::vector<DISPLAYCONFIG_MODE_INFO> modes(mode_count);
        const auto status = QueryDisplayConfig(QDC_ONLY_ACTIVE_PATHS, &path_count, paths.data(), &mode_count,
                                               modes.data(), nullptr);
        if (status == ERROR_SUCCESS) {
            paths.resize(path_count);
            return paths;
        }
        if (status != ERROR_INSUFFICIENT_BUFFER) {
            return {};
        }
    }
    return {};
}

bool exclusive_internal_source(const wchar_t *name, const std::vector<DISPLAYCONFIG_PATH_INFO> &paths) {
    unsigned int matches = 0;
    bool internal = false;
    for (const auto &path : paths) {
        DISPLAYCONFIG_SOURCE_DEVICE_NAME source{};
        source.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_SOURCE_NAME;
        source.header.size = sizeof(source);
        source.header.adapterId = path.sourceInfo.adapterId;
        source.header.id = path.sourceInfo.id;
        if (DisplayConfigGetDeviceInfo(&source.header) != ERROR_SUCCESS) {
            return false;
        }
        if (std::wcscmp(source.viewGdiDeviceName, name) != 0) {
            continue;
        }
        ++matches;
        internal = internal_output(path.targetInfo.outputTechnology);
    }
    return matches == 1 && internal;
}

std::optional<application::display::Display> read_display(const DISPLAY_DEVICEW &device,
                                                          const std::vector<DISPLAYCONFIG_PATH_INFO> &paths) {
    if (!(device.StateFlags & DISPLAY_DEVICE_ATTACHED_TO_DESKTOP)) {
        return {};
    }
    const auto mode = current_mode(device.DeviceName);
    if (!mode.dmSize) {
        return {};
    }
    return application::display::Display{utf8(device.DeviceName),
                                         utf8(device.DeviceString),
                                         mode.dmPelsWidth,
                                         mode.dmPelsHeight,
                                         mode.dmDisplayFrequency,
                                         rates(device.DeviceName, mode),
                                         exclusive_internal_source(device.DeviceName, paths)};
}

application::display::Change change_mode(const wchar_t *name, unsigned int hz) {
    using application::display::Status;
    auto mode = current_mode(name);
    if (!mode.dmSize) {
        return {Status::Unavailable, 0, "display-current-mode-unavailable"};
    }
    const auto available = rates(name, mode);
    if (std::ranges::find(available, hz) == available.end()) {
        return {Status::InvalidRequest, mode.dmDisplayFrequency, "refresh-not-in-enumerated-modes"};
    }
    mode.dmDisplayFrequency = hz;
    mode.dmFields = DM_DISPLAYFREQUENCY;
    if (ChangeDisplaySettingsExW(name, &mode, nullptr, CDS_TEST, nullptr) != DISP_CHANGE_SUCCESSFUL) {
        return {Status::Failed, 0, "display-mode-test-rejected"};
    }
    if (ChangeDisplaySettingsExW(name, &mode, nullptr, 0, nullptr) != DISP_CHANGE_SUCCESSFUL) {
        return {Status::Failed, 0, "display-mode-change-failed"};
    }
    const auto observed = current_mode(name);
    return {observed.dmDisplayFrequency == hz ? Status::Ready : Status::Unconfirmed,
            observed.dmDisplayFrequency, "display-mode-readback"};
}
} // namespace
application::display::Snapshot NativeRefresh::read() const {
    using application::display::Status;
    application::display::Snapshot result{Status::Ready, {}, "windows-enumerated-current-resolution"};
    const auto paths = active_paths();
    DISPLAY_DEVICEW device{};
    device.cb = sizeof(device);
    for (DWORD index = 0; EnumDisplayDevicesW(nullptr, index, &device, 0); ++index) {
        if (auto display = read_display(device, paths)) {
            result.displays.push_back(std::move(*display));
        }
    }
    if (result.displays.empty()) {
        result.status = Status::Unavailable;
    }
    return result;
}
application::display::Change NativeRefresh::set_refresh(const std::string &id, unsigned int hz) const {
    DISPLAY_DEVICEW device{};
    device.cb = sizeof(device);
    for (DWORD index = 0; EnumDisplayDevicesW(nullptr, index, &device, 0); ++index) {
        if ((device.StateFlags & DISPLAY_DEVICE_ATTACHED_TO_DESKTOP) && utf8(device.DeviceName) == id) {
            return change_mode(device.DeviceName, hz);
        }
    }
    return {application::display::Status::Unavailable, 0, "display-disconnected-or-unknown"};
}
} // namespace manager::adapters
