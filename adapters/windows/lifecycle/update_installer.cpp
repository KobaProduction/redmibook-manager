#include "adapters/updates/download_job.hpp"
#include <QCryptographicHash>
#include <array>
#include <objbase.h>
#include <shellapi.h>
#include <windows.h>

namespace manager::adapters {
namespace {
class LockedPackage final {
  public:
    explicit LockedPackage(const QString &path)
        : handle_(CreateFileW(reinterpret_cast<LPCWSTR>(path.utf16()), GENERIC_READ, FILE_SHARE_READ, nullptr,
                              OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr)) {}
    ~LockedPackage() {
        if (handle_ != INVALID_HANDLE_VALUE)
            CloseHandle(handle_);
    }
    LockedPackage(const LockedPackage &) = delete;
    LockedPackage &operator=(const LockedPackage &) = delete;
    bool verified(const DownloadedPackage &package) const {
        LARGE_INTEGER size{};
        if (handle_ == INVALID_HANDLE_VALUE || !GetFileSizeEx(handle_, &size) || size.QuadPart < 0 ||
            static_cast<std::uint64_t>(size.QuadPart) != package.size)
            return false;
        QCryptographicHash hash(QCryptographicHash::Sha256);
        std::array<char, 65536> bytes{};
        DWORD count{};
        do {
            if (!ReadFile(handle_, bytes.data(), static_cast<DWORD>(bytes.size()), &count, nullptr))
                return false;
            hash.addData(QByteArrayView(bytes.data(), static_cast<qsizetype>(count)));
        } while (count != 0);
        return hash.result().toHex() == package.digest;
    }

  private:
    HANDLE handle_;
};
application::InstallResult execute_installer(const QString &path) {
    SHELLEXECUTEINFOW execution{};
    execution.cbSize = sizeof(execution);
    execution.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_NOASYNC;
    execution.lpVerb = L"runas";
    execution.lpFile = reinterpret_cast<LPCWSTR>(path.utf16());
    execution.nShow = SW_SHOWNORMAL;
    if (!ShellExecuteExW(&execution))
        return GetLastError() == ERROR_CANCELLED ? application::InstallResult::Cancelled
                                                 : application::InstallResult::Failed;
    if (execution.hProcess != nullptr)
        CloseHandle(execution.hProcess);
    return application::InstallResult::Started;
}
} // namespace
application::InstallResult launch_update(const DownloadedPackage &package) {
    const LockedPackage file(package.path);
    if (!file.verified(package))
        return application::InstallResult::Failed;
    const auto initialized = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(initialized))
        return application::InstallResult::Failed;
    const auto result = execute_installer(package.path);
    CoUninitialize();
    return result;
}
} // namespace manager::adapters
