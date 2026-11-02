#include "adapters/windows/hosting/session_autostart.hpp"
#include <iostream>
#include <taskschd.h>
#include <windows.h>

namespace {
bool parse_task(ITaskService &service) {
    ITaskDefinition *task{};
    if (FAILED(service.NewTask(0, &task)))
        return false;
    const auto xml = manager::adapters::windows::session_autostart_xml(
        LR"(C:\Program Files\Koba & Production\Redmibook Manager\bin\redmibook-session.exe)");
    auto *document = SysAllocString(xml.c_str());
    const auto result = task->put_XmlText(document);
    SysFreeString(document);
    task->Release();
    if (FAILED(result))
        std::cerr << "Task Scheduler rejected sign-in definition: " << std::hex << result << '\n';
    return SUCCEEDED(result) && xml.find(L"Koba &amp; Production") != std::wstring::npos;
}
} // namespace
int main() {
    if (FAILED(CoInitializeEx(nullptr, COINIT_MULTITHREADED)))
        return 1;
    ITaskService *service{};
    const auto result = CoCreateInstance(CLSID_TaskScheduler, nullptr, CLSCTX_INPROC_SERVER, IID_ITaskService,
                                         reinterpret_cast<void **>(&service));
    bool valid = false;
    if (SUCCEEDED(result)) {
        const VARIANT empty{};
        valid = SUCCEEDED(service->Connect(empty, empty, empty, empty)) && parse_task(*service);
        service->Release();
    }
    CoUninitialize();
    return valid ? 0 : 1;
}
