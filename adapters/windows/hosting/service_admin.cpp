#include "adapters/windows/hosting/service_admin.hpp"
#include "adapters/windows/battery/policy_store.hpp"
#include "adapters/windows/hosting/client_pipe.hpp"
#include "adapters/windows/hosting/handle.hpp"
#include <array>
#include <string>
#include <winsvc.h>

namespace manager::adapters::windows {
namespace {
class ServiceHandle final {
  public:
    explicit ServiceHandle(SC_HANDLE value) : value_(value) {}
    ~ServiceHandle() {
        if (value_ != nullptr) {
            CloseServiceHandle(value_);
        }
    }
    ServiceHandle(const ServiceHandle &) = delete;
    ServiceHandle &operator=(const ServiceHandle &) = delete;
    [[nodiscard]] SC_HANDLE get() const {
        return value_;
    }

  private:
    SC_HANDLE value_;
};

bool wait_state(SC_HANDLE service, DWORD target) {
    for (int attempt = 0; attempt < 100; ++attempt) {
        SERVICE_STATUS status{};
        if (!QueryServiceStatus(service, &status)) {
            return false;
        }
        if (status.dwCurrentState == target) {
            return true;
        }
        Sleep(100);
    }
    return false;
}

bool stop(SC_HANDLE service) {
    SERVICE_STATUS status{};
    if (!ControlService(service, SERVICE_CONTROL_STOP, &status) &&
        GetLastError() != ERROR_SERVICE_NOT_ACTIVE) {
        return false;
    }
    return wait_state(service, SERVICE_STOPPED);
}

bool start(SC_HANDLE service) {
    if (!StartServiceW(service, 0, nullptr) && GetLastError() != ERROR_SERVICE_ALREADY_RUNNING) {
        return false;
    }
    if (!wait_state(service, SERVICE_RUNNING)) {
        return false;
    }
    for (int attempt = 0; attempt < 50; ++attempt) {
        if (probe_daemon().connected) {
            return true;
        }
        Sleep(100);
    }
    return false;
}

SC_HANDLE install(SC_HANDLE manager, bool &created) {
    std::array<wchar_t, 32768> path{};
    if (GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size())) == 0) {
        return nullptr;
    }
    const auto command = L"\"" + std::wstring(path.data()) + L"\" --service";
    auto service =
        CreateServiceW(manager, service_name, L"Redmibook Manager", SERVICE_ALL_ACCESS,
                       SERVICE_WIN32_OWN_PROCESS, SERVICE_AUTO_START, SERVICE_ERROR_NORMAL, command.c_str(),
                       nullptr, nullptr, nullptr, L"NT AUTHORITY\\LocalService", nullptr);
    created = service != nullptr;
    if (service == nullptr && GetLastError() == ERROR_SERVICE_EXISTS) {
        service = OpenServiceW(manager, service_name, SERVICE_ALL_ACCESS);
        if (service != nullptr &&
            !ChangeServiceConfigW(service, SERVICE_NO_CHANGE, SERVICE_NO_CHANGE, SERVICE_NO_CHANGE,
                                  command.c_str(), nullptr, nullptr, nullptr, nullptr, nullptr, nullptr)) {
            CloseServiceHandle(service);
            return nullptr;
        }
    }
    return service;
}

int execute(SC_HANDLE service, std::string_view command) {
    if (command == "--install" || command == "--start") {
        return start(service) ? 0 : 1;
    }
    if (command == "--stop") {
        return stop(service) ? 0 : 1;
    }
    if (command == "--restart") {
        return stop(service) && start(service) ? 0 : 1;
    }
    if (command == "--uninstall") {
        return stop(service) && DeleteService(service) ? 0 : 1;
    }
    return 2;
}
} // namespace

int service_command(std::string_view command) {
    if ((command == "--install" || command == "--configure") &&
        !windows_battery::provision_policy_directory())
        return 1;
    const ServiceHandle manager(
        OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT | SC_MANAGER_CREATE_SERVICE));
    if (manager.get() == nullptr) {
        return 1;
    }
    bool created = false;
    const ServiceHandle service(command == "--install" || command == "--configure"
                                    ? install(manager.get(), created)
                                    : OpenServiceW(manager.get(), service_name, SERVICE_ALL_ACCESS));
    if (service.get() == nullptr) {
        return command == "--uninstall" && GetLastError() == ERROR_SERVICE_DOES_NOT_EXIST ? 0 : 1;
    }
    const auto action = created && command == "--configure" ? "--start" : command;
    return action == "--configure" ? 0 : execute(service.get(), action);
}

bool service_running() {
    const ServiceHandle manager(OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT));
    const ServiceHandle service(OpenServiceW(manager.get(), service_name, SERVICE_QUERY_STATUS));
    SERVICE_STATUS status{};
    return service.get() != nullptr && QueryServiceStatus(service.get(), &status) &&
           status.dwCurrentState == SERVICE_RUNNING;
}
} // namespace manager::adapters::windows
