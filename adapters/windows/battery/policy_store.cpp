#include "adapters/windows/battery/policy_store.hpp"
#include "adapters/windows/hosting/handle.hpp"
#include <aclapi.h>
#include <fstream>
#include <iomanip>
#include <sddl.h>
#include <shlobj.h>
#include <sstream>

namespace manager::adapters::windows_battery {
namespace {
std::filesystem::path directory() {
    PWSTR raw{};
    if (FAILED(SHGetKnownFolderPath(FOLDERID_ProgramData, 0, nullptr, &raw)))
        return {};
    std::filesystem::path root(raw);
    CoTaskMemFree(raw);
    return root / L"Koba Production" / L"Redmibook Manager" / L"service";
}
bool protect_directory(const std::filesystem::path &path) {
    PSECURITY_DESCRIPTOR descriptor{};
    if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(
            L"D:P(A;OICI;FA;;;SY)(A;OICI;FA;;;BA)(A;OICI;FA;;;LS)", SDDL_REVISION_1, &descriptor, nullptr))
        return false;
    PACL acl{};
    BOOL present{}, inherited{};
    const bool read = GetSecurityDescriptorDacl(descriptor, &present, &acl, &inherited) && present;
    const auto result =
        read ? SetNamedSecurityInfoW(const_cast<wchar_t *>(path.c_str()), SE_FILE_OBJECT,
                                     DACL_SECURITY_INFORMATION | PROTECTED_DACL_SECURITY_INFORMATION, nullptr,
                                     nullptr, acl, nullptr)
             : ERROR_INVALID_ACL;
    LocalFree(descriptor);
    return result == ERROR_SUCCESS;
}
application::battery::Policy parse_policy(const std::filesystem::path &path, bool unsettled) {
    std::ifstream file(path);
    int value{};
    std::string marker, extra;
    if (!(file >> marker >> value) || marker != "RMB-CHARGE-1" || (value != 80 && value != 100) ||
        (file >> extra))
        return {{}, false, unsettled};
    return {static_cast<application::battery::ChargeLimit>(value), true, unsettled};
}
} // namespace
bool provision_policy_directory() {
    const auto path = directory();
    if (path.empty())
        return false;
    std::error_code error;
    std::filesystem::create_directories(path, error);
    return !error && protect_directory(path);
}
application::battery::Policy PolicyStore::load() {
    return load_policy(directory());
}
application::battery::Policy load_policy(const std::filesystem::path &root) {
    if (root.empty())
        return {{}, false, true};
    const auto path = root / L"charge-policy.v1";
    std::error_code error;
    const bool unsettled = std::filesystem::exists(root / L"charge-write.pending", error);
    if (error)
        return {{}, false, true};
    const bool exists = std::filesystem::exists(path, error);
    if (error)
        return {{}, false, true};
    if (!exists)
        return {{}, true, unsettled};
    return parse_policy(path, unsettled);
}
bool PolicyStore::save(application::battery::ChargeLimit limit) {
    const auto root = directory();
    if (root.empty())
        return false;
    const auto temporary = root / L"charge-policy.v1.tmp";
    const std::string text = "RMB-CHARGE-1 " + std::to_string(static_cast<int>(limit)) + "\n";
    {
        windows::Handle file(CreateFileW(temporary.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                                         FILE_ATTRIBUTE_NORMAL, nullptr));
        if (!file.valid())
            return false;
        DWORD written{};
        if (!WriteFile(file.get(), text.data(), static_cast<DWORD>(text.size()), &written, nullptr) ||
            written != text.size() || !FlushFileBuffers(file.get()))
            return false;
    }
    return MoveFileExW(temporary.c_str(), (root / L"charge-policy.v1").c_str(),
                       MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != FALSE;
}
bool PolicyStore::mark_unsettled(bool value) {
    const auto root = directory();
    if (root.empty())
        return false;
    const auto path = root / L"charge-write.pending";
    if (!value)
        return DeleteFileW(path.c_str()) || GetLastError() == ERROR_FILE_NOT_FOUND;
    windows::Handle file(
        CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr));
    return file.valid() && FlushFileBuffers(file.get());
}
namespace {
std::string encode_policy_event(std::string_view reason, const application::battery::Observation &before,
                                std::optional<application::battery::ChargeLimit> desired,
                                const application::battery::Observation &after) {
    SYSTEMTIME time{};
    GetSystemTime(&time);
    std::ostringstream line;
    line << "{\"utc\":\"" << std::setfill('0') << std::setw(4) << time.wYear << '-' << std::setw(2)
         << time.wMonth << '-' << std::setw(2) << time.wDay << 'T' << std::setw(2) << time.wHour << ':'
         << std::setw(2) << time.wMinute << ':' << std::setw(2) << time.wSecond << "Z\",\"reason\":\""
         << reason
         << "\",\"observed\":" << (before.limit ? std::to_string(static_cast<int>(*before.limit)) : "null")
         << ",\"desired\":" << (desired ? std::to_string(static_cast<int>(*desired)) : "null")
         << ",\"after\":" << (after.limit ? std::to_string(static_cast<int>(*after.limit)) : "null")
         << ",\"readStatus\":" << static_cast<int>(before.status)
         << ",\"resultStatus\":" << static_cast<int>(after.status) << "}\n";
    return line.str();
}
} // namespace
void PolicyLog::record(std::string_view reason, const application::battery::Observation &before,
                       std::optional<application::battery::ChargeLimit> desired,
                       const application::battery::Observation &after) noexcept {
    try {
        const auto root = directory();
        if (root.empty())
            return;
        const auto path = root / L"charge-events.jsonl";
        std::error_code error;
        if (std::filesystem::file_size(path, error) > 524288 && !error)
            MoveFileExW(path.c_str(), (root / L"charge-events.previous.jsonl").c_str(),
                        MOVEFILE_REPLACE_EXISTING);
        const auto line = encode_policy_event(reason, before, desired, after);
        std::ofstream file(path, std::ios::app);
        file << line;
        file.flush();
        last_ = file ? line : "audit-write-failed";
        if (!file)
            OutputDebugStringA("Redmibook charge audit write failed\n");
    } catch (...) {
        OutputDebugStringA("Redmibook charge audit unavailable\n");
    }
}
} // namespace manager::adapters::windows_battery
