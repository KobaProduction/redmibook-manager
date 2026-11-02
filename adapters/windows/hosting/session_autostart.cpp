#include "adapters/windows/hosting/session_autostart.hpp"
#include <array>
#include <memory>
#include <taskschd.h>
#include <windows.h>

namespace manager::adapters::windows {
namespace {
constexpr auto task_name = L"KobaProduction.RedmibookManager.Session";
struct Release {
    void operator()(IUnknown *value) const {
        if (value)
            value->Release();
    }
};
template <class T> using Com = std::unique_ptr<T, Release>;
struct FreeText {
    void operator()(wchar_t *value) const {
        SysFreeString(value);
    }
};
using Text = std::unique_ptr<wchar_t, FreeText>;
Text text(const wchar_t *value) {
    return Text(SysAllocString(value));
}
class Apartment final {
  public:
    Apartment() : status(CoInitializeEx(nullptr, COINIT_MULTITHREADED)) {}
    ~Apartment() {
        if (SUCCEEDED(status))
            CoUninitialize();
    }
    Apartment(const Apartment &) = delete;
    Apartment &operator=(const Apartment &) = delete;
    HRESULT status;
};
std::wstring escape_xml(const std::wstring &value) {
    std::wstring result;
    for (const auto character : value) {
        if (character == L'&')
            result += L"&amp;";
        else if (character == L'<')
            result += L"&lt;";
        else if (character == L'>')
            result += L"&gt;";
        else
            result += character;
    }
    return result;
}
Com<ITaskFolder> task_folder() {
    ITaskService *raw_service{};
    if (FAILED(CoCreateInstance(CLSID_TaskScheduler, nullptr, CLSCTX_INPROC_SERVER, IID_ITaskService,
                                reinterpret_cast<void **>(&raw_service))))
        return {};
    const Com<ITaskService> service(raw_service);
    const VARIANT empty{};
    if (FAILED(service->Connect(empty, empty, empty, empty)))
        return {};
    ITaskFolder *raw_folder{};
    if (FAILED(service->GetFolder(text(L"\\").get(), &raw_folder)))
        return {};
    return Com<ITaskFolder>(raw_folder);
}
int register_task(ITaskFolder &folder) {
    std::array<wchar_t, 32768> module{};
    const auto count = GetModuleFileNameW(nullptr, module.data(), static_cast<DWORD>(module.size()));
    if (count == 0 || count >= module.size())
        return 1;
    const auto executable = std::filesystem::path(module.data()).parent_path() / L"redmibook-session.exe";
    if (!std::filesystem::is_regular_file(executable))
        return 1;
    const auto xml = text(session_autostart_xml(executable).c_str());
    const auto group = text(L"S-1-5-32-545");
    VARIANT principal{};
    principal.vt = VT_BSTR;
    principal.bstrVal = group.get();
    const VARIANT empty{};
    IRegisteredTask *raw_task{};
    const auto result = folder.RegisterTask(text(task_name).get(), xml.get(), TASK_CREATE_OR_UPDATE,
                                            principal, empty, TASK_LOGON_GROUP, empty, &raw_task);
    const Com<IRegisteredTask> task(raw_task);
    VARIANT_BOOL enabled{};
    return SUCCEEDED(result) && task && SUCCEEDED(task->get_Enabled(&enabled)) && enabled == VARIANT_TRUE ? 0
                                                                                                          : 1;
}
} // namespace

std::wstring session_autostart_xml(const std::filesystem::path &executable) {
    return LR"(<Task version="1.2" xmlns="http://schemas.microsoft.com/windows/2004/02/mit/task">
<RegistrationInfo><Author>Koba Production</Author>
<Description>RedmiBook Manager hotkeys, OSD and optional tray at Windows sign-in.</Description></RegistrationInfo>
<Triggers><LogonTrigger><Enabled>true</Enabled></LogonTrigger></Triggers>
<Principals><Principal id="User"><GroupId>S-1-5-32-545</GroupId><RunLevel>LeastPrivilege</RunLevel></Principal></Principals>
<Settings><MultipleInstancesPolicy>Parallel</MultipleInstancesPolicy>
<DisallowStartIfOnBatteries>false</DisallowStartIfOnBatteries><StopIfGoingOnBatteries>false</StopIfGoingOnBatteries>
<AllowHardTerminate>false</AllowHardTerminate><StartWhenAvailable>true</StartWhenAvailable>
<RunOnlyIfNetworkAvailable>false</RunOnlyIfNetworkAvailable><Enabled>true</Enabled>
<RunOnlyIfIdle>false</RunOnlyIfIdle><ExecutionTimeLimit>PT0S</ExecutionTimeLimit></Settings>
<Actions Context="User"><Exec><Command>)" +
           escape_xml(executable.wstring()) +
           LR"(</Command><Arguments>--with-tray</Arguments><WorkingDirectory>)" +
           escape_xml(executable.parent_path().wstring()) + LR"(</WorkingDirectory></Exec></Actions></Task>)";
}
int session_autostart_command(bool remove) {
    const Apartment apartment;
    if (FAILED(apartment.status))
        return 1;
    const auto folder = task_folder();
    if (!folder)
        return 1;
    if (!remove)
        return register_task(*folder);
    const auto result = folder->DeleteTask(text(task_name).get(), 0);
    return SUCCEEDED(result) || result == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND) ? 0 : 1;
}
} // namespace manager::adapters::windows
