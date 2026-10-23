#include "adapters/platform/power_controls.hpp"
#include <windows.h>

#include <memory>
#include <mutex>
#include <oleauto.h>
#include <wbemidl.h>

namespace manager::adapters {
namespace {
// A client deadline cannot cancel an outstanding provider write. Fail closed
// across all adapter instances in this process after an uncertain write.
std::mutex write_mutex;
bool write_unsettled{false};
DWORD pending_cookie{};
using application::display::Brightness;
using application::display::BrightnessStatus;
struct Failure {
    HRESULT code;
};
void require(HRESULT code) {
    if (FAILED(code))
        throw Failure{code};
}
template <class T> struct Release {
    void operator()(T *value) const {
        if (value)
            value->Release();
    }
};
template <class T> using Com = std::unique_ptr<T, Release<T>>;
Com<IGlobalInterfaceTable> interface_table() {
    IGlobalInterfaceTable *raw{};
    require(CoCreateInstance(CLSID_StdGlobalInterfaceTable, nullptr, CLSCTX_INPROC_SERVER,
                             IID_IGlobalInterfaceTable, reinterpret_cast<void **>(&raw)));
    return Com<IGlobalInterfaceTable>(raw);
}
void reconcile_write() {
    if (!pending_cookie)
        return;
    const auto table = interface_table();
    IWbemCallResult *raw{};
    require(
        table->GetInterfaceFromGlobal(pending_cookie, IID_IWbemCallResult, reinterpret_cast<void **>(&raw)));
    const Com<IWbemCallResult> call(raw);
    LONG status{};
    if (call->GetCallStatus(0, &status) != WBEM_S_NO_ERROR)
        return;
    require(table->RevokeInterfaceFromGlobal(pending_cookie));
    pending_cookie = 0;
    write_unsettled = false;
}
struct Apartment {
    HRESULT code{CoInitializeEx(nullptr, COINIT_MULTITHREADED)};
    Apartment() {
        if (code != RPC_E_CHANGED_MODE)
            require(code);
    }
    ~Apartment() {
        if (SUCCEEDED(code))
            CoUninitialize();
    }
};
struct Text {
    BSTR value;
    explicit Text(const wchar_t *text) : value(SysAllocString(text)) {
        if (!value)
            throw Failure{E_OUTOFMEMORY};
    }
    ~Text() {
        SysFreeString(value);
    }
    Text(const Text &) = delete;
    Text &operator=(const Text &) = delete;
};
struct Variant {
    VARIANT value{};
    Variant() {
        VariantInit(&value);
    }
    ~Variant() {
        VariantClear(&value);
    }
    Variant(const Variant &) = delete;
    Variant &operator=(const Variant &) = delete;
};
Com<IWbemServices> connect_brightness() {
    IWbemLocator *raw{};
    require(CoCreateInstance(CLSID_WbemLocator, nullptr, CLSCTX_INPROC_SERVER, IID_IWbemLocator,
                             reinterpret_cast<void **>(&raw)));
    Com<IWbemLocator> locator(raw);
    IWbemServices *services{};
    const Text space(L"ROOT\\WMI");
    require(locator->ConnectServer(space.value, nullptr, nullptr, nullptr, WBEM_FLAG_CONNECT_USE_MAX_WAIT,
                                   nullptr, nullptr, &services));
    Com<IWbemServices> result(services);
    require(CoSetProxyBlanket(services, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, nullptr, RPC_C_AUTHN_LEVEL_CALL,
                              RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, EOAC_NONE));
    return result;
}
Com<IWbemClassObject> unique_monitor(IWbemServices &services, const wchar_t *query) {
    const Text language(L"WQL"), statement(query);
    IEnumWbemClassObject *raw{};
    require(services.ExecQuery(language.value, statement.value,
                               WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, nullptr, &raw));
    Com<IEnumWbemClassObject> rows(raw);
    IWbemClassObject *first{};
    ULONG count{};
    const auto status = rows->Next(1500, 1, &first, &count);
    Com<IWbemClassObject> result(first);
    require(status);
    if (status != WBEM_S_NO_ERROR || count != 1)
        return {};
    IWbemClassObject *second{};
    const auto end = rows->Next(1500, 1, &second, &count);
    Com<IWbemClassObject> extra(second);
    return end == WBEM_S_FALSE && count == 0 ? std::move(result) : Com<IWbemClassObject>{};
}
Brightness observe(IWbemServices &services) {
    auto monitor = unique_monitor(services, L"SELECT * FROM WmiMonitorBrightness WHERE Active=TRUE");
    if (!monitor)
        return {};
    Variant level, number;
    require(monitor->Get(L"CurrentBrightness", 0, &level.value, nullptr, nullptr));
    require(VariantChangeType(&number.value, &level.value, 0, VT_UI4));
    if (number.value.ulVal > 100)
        return {};
    return {BrightnessStatus::Ready, number.value.ulVal};
}
Com<IWbemClassObject> brightness_parameters(IWbemServices &services, unsigned int percent) {
    const Text name(L"WmiMonitorBrightnessMethods");
    IWbemClassObject *raw{}, *method{}, *instance{};
    require(services.GetObject(name.value, 0, nullptr, &raw, nullptr));
    Com<IWbemClassObject> type(raw);
    require(type->GetMethod(L"WmiSetBrightness", 0, &method, nullptr));
    Com<IWbemClassObject> definition(method);
    require(definition->SpawnInstance(0, &instance));
    Com<IWbemClassObject> input(instance);
    Variant value;
    value.value.vt = VT_I4;
    value.value.lVal = 0;
    require(input->Put(L"Timeout", 0, &value.value, 0));
    value.value.lVal = static_cast<LONG>(percent);
    require(input->Put(L"Brightness", 0, &value.value, 0));
    return input;
}
bool confirm_write(IWbemCallResult &call) {
    const auto table = interface_table();
    require(table->RegisterInterfaceInGlobal(&call, IID_IWbemCallResult, &pending_cookie));
    LONG method_status{};
    const auto completion = call.GetCallStatus(1500, &method_status);
    require(completion);
    if (completion == WBEM_S_NO_ERROR) {
        require(table->RevokeInterfaceFromGlobal(pending_cookie));
        pending_cookie = 0;
        write_unsettled = false;
    }
    return completion == WBEM_S_NO_ERROR && SUCCEEDED(method_status);
}
bool write_brightness(IWbemServices &services, unsigned int percent) {
    auto monitor = unique_monitor(services, L"SELECT * FROM WmiMonitorBrightnessMethods WHERE Active=TRUE");
    if (!monitor)
        return false;
    Variant path;
    require(monitor->Get(L"__PATH", 0, &path.value, nullptr, nullptr));
    if (path.value.vt != VT_BSTR)
        return false;
    auto input = brightness_parameters(services, percent);
    IWbemCallResult *raw{};
    const Text method(L"WmiSetBrightness");
    write_unsettled = true;
    require(services.ExecMethod(path.value.bstrVal, method.value, WBEM_FLAG_RETURN_IMMEDIATELY, nullptr,
                                input.get(), nullptr, &raw));
    Com<IWbemCallResult> call(raw);
    if (!call)
        return false;
    return confirm_write(*call);
}
Brightness failed(HRESULT code, bool write) {
    if (code == E_ACCESSDENIED || code == static_cast<HRESULT>(WBEM_E_ACCESS_DENIED))
        return {BrightnessStatus::Denied, {}};
    return {write ? BrightnessStatus::Unconfirmed : BrightnessStatus::Failed, {}};
}
Brightness apply_brightness(unsigned int percent) {
    const auto services = connect_brightness();
    if (observe(*services).status != BrightnessStatus::Ready)
        return {};
    if (!write_brightness(*services, percent))
        return {BrightnessStatus::Unconfirmed, {}};
    auto after = observe(*services);
    if (after.percent != percent)
        after.status = BrightnessStatus::Unconfirmed;
    if (after.status == BrightnessStatus::Ready)
        write_unsettled = false;
    return after;
}
class NativeBrightness final : public application::display::BrightnessPort {
  public:
    Brightness read() const override {
        const std::lock_guard lock(write_mutex);
        try {
            const Apartment apartment;
            reconcile_write();
            const auto services = connect_brightness();
            return observe(*services);
        } catch (const Failure &error) {
            return failed(error.code, false);
        }
    }
    Brightness set(unsigned int percent) override {
        const std::lock_guard lock(write_mutex);
        if (percent > 100)
            return {BrightnessStatus::Failed, {}};
        try {
            const Apartment apartment;
            reconcile_write();
            if (write_unsettled)
                return {BrightnessStatus::Unconfirmed, {}};
            return apply_brightness(percent);
        } catch (const Failure &error) {
            return failed(error.code, true);
        }
    }
};
} // namespace
std::shared_ptr<application::display::BrightnessPort> make_brightness() {
    return std::make_shared<NativeBrightness>();
}
} // namespace manager::adapters
