#include "adapters/windows/hosting/handle.hpp"
#include "adapters/windows/hosting/payload_manifest.hpp"
#include "adapters/windows/hosting/service_admin.hpp"
#include <filesystem>
#include <fstream>
#include <restartmanager.h>
#include <string>
#include <vector>
#include <windows.h>

namespace manager::adapters::windows {
namespace {
class RestartSession final {
  public:
    DWORD value{0};
    DWORD result{ERROR_INVALID_HANDLE};
    RestartSession() {
        wchar_t key[CCH_RM_SESSION_KEY + 1]{};
        result = RmStartSession(&value, 0, key);
    }
    ~RestartSession() {
        if (result == ERROR_SUCCESS) {
            RmEndSession(value);
        }
    }
    RestartSession(const RestartSession &) = delete;
    RestartSession &operator=(const RestartSession &) = delete;
};
std::vector<std::wstring> payload_files(const std::filesystem::path &directory) {
    std::vector<std::wstring> result;
    for (const auto &entry : std::filesystem::directory_iterator(directory)) {
        if (entry.is_regular_file() &&
            (entry.path().extension() == L".exe" || entry.path().extension() == L".dll")) {
            result.push_back(entry.path().wstring());
        }
    }
    return result;
}

DWORD register_payload(DWORD session, const std::vector<std::wstring> &files) {
    std::vector<const wchar_t *> names;
    names.reserve(files.size());
    for (const auto &file : files) {
        names.push_back(file.c_str());
    }
    return RmRegisterResources(session, static_cast<UINT>(names.size()), names.data(), 0, nullptr, 0,
                               nullptr);
}

void marker_file(const std::filesystem::path &base, const wchar_t *suffix, DWORD code) {
    const std::filesystem::path destination(base.wstring() + suffix);
    const std::filesystem::path temporary(destination.wstring() + L".tmp");
    {
        std::ofstream file(temporary);
        file << code;
    }
    std::filesystem::rename(temporary, destination);
}

DWORD finalize_update(DWORD session, const MaintenanceRequest &request, const PayloadManifest &previous,
                      bool was_running) {
    std::ifstream input(std::filesystem::path(request.marker.wstring() + L".commit"));
    std::string decision;
    input >> decision;
    if (decision != "commit") {
        RmRestart(session, 0, nullptr);
        return ERROR_CANCELLED;
    }
    const bool cleaned = prune_obsolete(request.directory.parent_path(), previous);
    const auto restart = RmRestart(session, 0, nullptr);
    if (was_running && service_command("--start") != 0) {
        return ERROR_SERVICE_NOT_ACTIVE;
    }
    return cleaned ? restart : ERROR_WRITE_FAULT;
}

DWORD await_commit(DWORD session, const MaintenanceRequest &request, const PayloadManifest &previous,
                   bool was_running) {
    marker_file(request.marker, L".ready", 0);
    for (int count = 0; count < 1800; ++count) {
        if (std::filesystem::exists(request.marker.wstring() + L".commit")) {
            return finalize_update(session, request, previous, was_running);
        }
        if (std::filesystem::exists(request.marker.wstring() + L".remove")) {
            return ERROR_SUCCESS;
        }
        Sleep(100);
    }
    RmRestart(session, 0, nullptr);
    return ERROR_TIMEOUT;
}
} // namespace

int maintain_installation(const MaintenanceRequest &request) {
    const RestartSession session;
    DWORD result = session.result;
    const bool was_running = service_running();
    try {
        const auto previous = read_manifest(request.directory.parent_path());
        if (result == ERROR_SUCCESS) {
            result = register_payload(session.value, payload_files(request.directory));
            if (result == ERROR_SUCCESS) {
                result = RmShutdown(session.value, 0, nullptr);
            }
            if (result == ERROR_SUCCESS) {
                result = await_commit(session.value, request, previous, was_running);
            } else {
                RmRestart(session.value, 0, nullptr);
            }
        }
    } catch (const std::exception &) {
        RmRestart(session.value, 0, nullptr);
        result = ERROR_INSTALL_FAILURE;
    }
    marker_file(request.marker, L".done", result);
    return result == ERROR_SUCCESS ? 0 : 1;
}
} // namespace manager::adapters::windows
