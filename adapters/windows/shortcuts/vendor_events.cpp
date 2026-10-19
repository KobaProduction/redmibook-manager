#include "adapters/windows/shortcuts/vendor_events.hpp"

#include <windows.h>

#include <oleauto.h>
#include <wbemidl.h>

#include <algorithm>
#include <chrono>
#include <memory>

namespace manager::adapters::windows_shortcuts {
namespace {

struct Failure {
    HRESULT result;
    const char *stage;
};

void require(HRESULT result, const char *stage) {
    if (FAILED(result))
        throw Failure{result, stage};
}

template <typename T> struct Release {
    void operator()(T *object) const noexcept {
        if (object != nullptr)
            object->Release();
    }
};
template <typename T> using Com = std::unique_ptr<T, Release<T>>;

class Apartment {
  public:
    Apartment() : result_(CoInitializeEx(nullptr, COINIT_MULTITHREADED)) {
        if (result_ != RPC_E_CHANGED_MODE)
            require(result_, "Initialize vendor-event COM");
    }
    ~Apartment() {
        if (SUCCEEDED(result_))
            CoUninitialize();
    }
    Apartment(const Apartment &) = delete;
    Apartment &operator=(const Apartment &) = delete;

  private:
    HRESULT result_;
};

class Text {
  public:
    explicit Text(const wchar_t *text) : value_(SysAllocString(text)) {
        if (value_ == nullptr)
            throw Failure{E_OUTOFMEMORY, "Allocate vendor-event query text"};
    }
    ~Text() {
        SysFreeString(value_);
    }
    Text(const Text &) = delete;
    Text &operator=(const Text &) = delete;
    [[nodiscard]] BSTR get() const {
        return value_;
    }

  private:
    BSTR value_;
};

class Variant {
  public:
    Variant() {
        VariantInit(&value);
    }
    ~Variant() {
        VariantClear(&value);
    }
    Variant(const Variant &) = delete;
    Variant &operator=(const Variant &) = delete;
    VARIANT value{};
};

Com<IWbemServices> connect() {
    IWbemLocator *raw_locator = nullptr;
    require(CoCreateInstance(CLSID_WbemLocator, nullptr, CLSCTX_INPROC_SERVER, IID_IWbemLocator,
                             reinterpret_cast<void **>(&raw_locator)),
            "Create vendor-event WMI locator");
    const Com<IWbemLocator> locator(raw_locator);
    IWbemServices *raw_services = nullptr;
    const Text space(L"ROOT\\WMI");
    const auto connected =
        locator->ConnectServer(space.get(), nullptr, nullptr, nullptr, WBEM_FLAG_CONNECT_USE_MAX_WAIT,
                               nullptr, nullptr, &raw_services);
    Com<IWbemServices> services(raw_services);
    require(connected, "Connect vendor-event ROOT/WMI");
    require(CoSetProxyBlanket(services.get(), RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, nullptr,
                              RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, EOAC_NONE),
            "Set vendor-event WMI identity");
    return services;
}

Com<IEnumWbemClassObject> subscribe(IWbemServices &services) {
    const Text language(L"WQL");
    const Text query(L"SELECT * FROM HID_EVENT20");
    IEnumWbemClassObject *raw_events = nullptr;
    const auto subscribed = services.ExecNotificationQuery(
        language.get(), query.get(), WBEM_FLAG_RETURN_IMMEDIATELY | WBEM_FLAG_FORWARD_ONLY, nullptr,
        &raw_events);
    Com<IEnumWbemClassObject> events(raw_events);
    require(subscribed, "Subscribe HID_EVENT20");
    if (!events)
        throw Failure{static_cast<HRESULT>(WBEM_E_FAILED), "Missing HID_EVENT20 subscription"};
    return events;
}

Com<IWbemClassObject> await_event(IEnumWbemClassObject &events, long timeout_ms) {
    IWbemClassObject *raw_object = nullptr;
    ULONG returned = 0;
    const auto waited = events.Next(timeout_ms, 1, &raw_object, &returned);
    Com<IWbemClassObject> object(raw_object);
    require(waited, "Wait for HID_EVENT20");
    if (returned == 0 && (waited == WBEM_S_TIMEDOUT || waited == WBEM_S_FALSE))
        return {};
    if (returned != 1 || !object)
        throw Failure{static_cast<HRESULT>(WBEM_E_FAILED), "Invalid HID_EVENT20 enumeration result"};
    return object;
}

std::vector<std::uint8_t> payload(IWbemClassObject &event) {
    Variant detail;
    require(event.Get(L"EventDetail", 0, &detail.value, nullptr, nullptr), "Read HID_EVENT20 EventDetail");
    if (detail.value.vt != (VT_ARRAY | VT_UI1) || detail.value.parray == nullptr ||
        SafeArrayGetDim(detail.value.parray) != 1)
        throw Failure{static_cast<HRESULT>(WBEM_E_TYPE_MISMATCH), "Invalid HID_EVENT20 payload type"};
    long lower = 0;
    long upper = 0;
    require(SafeArrayGetLBound(detail.value.parray, 1, &lower), "Read vendor-event array bound");
    require(SafeArrayGetUBound(detail.value.parray, 1, &upper), "Read vendor-event array bound");
    const auto length = static_cast<long long>(upper) - static_cast<long long>(lower) + 1;
    if (length < 1 || length > 64)
        throw Failure{static_cast<HRESULT>(WBEM_E_TYPE_MISMATCH),
                      "Vendor-event payload exceeds diagnostic bound"};
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(length));
    void *data = nullptr;
    require(SafeArrayAccessData(detail.value.parray, &data), "Access vendor-event payload");
    std::copy_n(static_cast<const std::uint8_t *>(data), bytes.size(), bytes.begin());
    require(SafeArrayUnaccessData(detail.value.parray), "Release vendor-event payload");
    return bytes;
}

VendorEventReport observe(IEnumWbemClassObject &events, unsigned int timeout_ms) {
    using Clock = std::chrono::steady_clock;
    const auto started = Clock::now();
    const auto deadline = started + std::chrono::milliseconds(timeout_ms);
    VendorEventReport report{VendorProbeStatus::Completed, true, false, {}, "Passive observation completed"};
    while (report.events.size() < 64) {
        const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - Clock::now());
        if (remaining.count() <= 0)
            break;
        const auto event = await_event(events, static_cast<long>(remaining.count()));
        if (!event)
            break;
        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - started);
        report.events.push_back({static_cast<std::uint64_t>(elapsed.count()), payload(*event)});
    }
    report.event_limit_reached = report.events.size() == 64;
    return report;
}

VendorProbeStatus failure_status(HRESULT result) {
    if (result == E_ACCESSDENIED || result == static_cast<HRESULT>(WBEM_E_ACCESS_DENIED))
        return VendorProbeStatus::PermissionDenied;
    if (result == static_cast<HRESULT>(WBEM_E_INVALID_CLASS) ||
        result == static_cast<HRESULT>(WBEM_E_INVALID_NAMESPACE))
        return VendorProbeStatus::Unavailable;
    return VendorProbeStatus::Failed;
}

} // namespace

VendorEventReport probe_vendor_events(unsigned int timeout_ms, const std::function<void()> &ready) {
    if (timeout_ms < 1 || timeout_ms > 60000)
        return {
            VendorProbeStatus::InvalidRequest, false, false, {}, "Observation window must be 1..60000 ms"};
    bool listening = false;
    try {
        const Apartment apartment;
        const auto services = connect();
        const auto events = subscribe(*services);
        listening = true;
        if (ready)
            ready();
        return observe(*events, timeout_ms);
    } catch (const Failure &failure) {
        return {failure_status(failure.result),
                listening,
                false,
                {},
                std::string(failure.stage) + " (HRESULT " + std::to_string(failure.result) + ")"};
    }
}

} // namespace manager::adapters::windows_shortcuts
