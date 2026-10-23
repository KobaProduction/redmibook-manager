#include "adapters/windows/firmware/mifs_transport.hpp"
#include "adapters/windows/firmware/native_result.hpp"

#include <windows.h>

#include <oleauto.h>
#include <wbemidl.h>

#include <algorithm>
#include <memory>
#include <stdexcept>
#include <utility>

namespace manager::adapters::windows_firmware {
namespace {

constexpr long timeout_ms = 3000;

struct Failure {
    HRESULT result;
    const char *stage;
};

void require(HRESULT result, const char *stage) {
    if (FAILED(result))
        throw Failure{result, stage};
}

template <typename T> struct Release {
    void operator()(T *value) const noexcept {
        if (value != nullptr)
            value->Release();
    }
};

template <typename T> using Com = std::unique_ptr<T, Release<T>>;

class Apartment {
  public:
    Apartment() : result_(CoInitializeEx(nullptr, COINIT_MULTITHREADED)) {
        if (result_ != RPC_E_CHANGED_MODE)
            require(result_, "Initialize COM");
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

class Text {
  public:
    explicit Text(const wchar_t *text) : value_(SysAllocString(text)) {
        if (value_ == nullptr)
            throw Failure{E_OUTOFMEMORY, "Allocate WMI text"};
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

Com<IWbemServices> connect() {
    IWbemLocator *raw_locator = nullptr;
    require(CoCreateInstance(CLSID_WbemLocator, nullptr, CLSCTX_INPROC_SERVER, IID_IWbemLocator,
                             reinterpret_cast<void **>(&raw_locator)),
            "Create WMI locator");
    const Com<IWbemLocator> locator(raw_locator);
    IWbemServices *raw_services = nullptr;
    const Text space(L"ROOT\\WMI");
    require(locator->ConnectServer(space.get(), nullptr, nullptr, nullptr, WBEM_FLAG_CONNECT_USE_MAX_WAIT,
                                   nullptr, nullptr, &raw_services),
            "Connect ROOT/WMI");
    Com<IWbemServices> services(raw_services);
    require(CoSetProxyBlanket(services.get(), RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, nullptr,
                              RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, EOAC_NONE),
            "Set WMI identity");
    return services;
}

bool mifs_name(IWbemClassObject &object) {
    Variant name;
    require(object.Get(L"InstanceName", 0, &name.value, nullptr, nullptr), "Read MIFS instance name");
    if (name.value.vt != VT_BSTR || name.value.bstrVal == nullptr)
        return false;
    const std::wstring text(name.value.bstrVal, SysStringLen(name.value.bstrVal));
    const auto slash = text.find_last_of(L"\\/");
    const auto component = text.substr(slash == std::wstring::npos ? 0 : slash + 1);
    return _wcsicmp(component.c_str(), L"MIFS") == 0 || _wcsnicmp(component.c_str(), L"MIFS_", 5) == 0;
}

Com<IEnumWbemClassObject> query_instances(IWbemServices &services) {
    const Text language(L"WQL");
    const Text query(L"SELECT * FROM MICommonInterface WHERE Active=TRUE");
    IEnumWbemClassObject *raw_objects = nullptr;
    require(services.ExecQuery(language.get(), query.get(),
                               WBEM_FLAG_RETURN_IMMEDIATELY | WBEM_FLAG_FORWARD_ONLY, nullptr, &raw_objects),
            "Query active MIFS instance");
    return Com<IEnumWbemClassObject>(raw_objects);
}

Com<IWbemClassObject> next_instance(IEnumWbemClassObject &objects) {
    IWbemClassObject *raw_object = nullptr;
    ULONG returned = 0;
    const auto next = objects.Next(timeout_ms, 1, &raw_object, &returned);
    Com<IWbemClassObject> object(raw_object);
    if (next == WBEM_S_TIMEDOUT)
        throw Failure{static_cast<HRESULT>(WBEM_E_TIMED_OUT), "MIFS instance enumeration timeout"};
    require(next, "Enumerate MIFS instances");
    return object;
}

Com<IWbemClassObject> instance(IWbemServices &services) {
    const auto objects = query_instances(services);
    Com<IWbemClassObject> selected;
    for (unsigned int count = 0; count < 32; ++count) {
        auto object = next_instance(*objects);
        if (!object)
            return selected;
        if (mifs_name(*object)) {
            if (selected)
                throw Failure{static_cast<HRESULT>(WBEM_E_FAILED), "Ambiguous active MIFS instances"};
            selected = std::move(object);
        }
    }
    throw Failure{static_cast<HRESULT>(WBEM_E_FAILED), "Too many MIFS instances"};
}

Com<IWbemClassObject> parameters(IWbemServices &services) {
    IWbemClassObject *raw_class = nullptr;
    const Text name(L"MICommonInterface");
    require(services.GetObject(name.get(), 0, nullptr, &raw_class, nullptr), "Read MIFS class");
    const Com<IWbemClassObject> type(raw_class);
    IWbemClassObject *raw_definition = nullptr;
    require(type->GetMethod(L"MiInterface", 0, &raw_definition, nullptr), "Read MIFS method");
    const Com<IWbemClassObject> definition(raw_definition);
    IWbemClassObject *raw_parameters = nullptr;
    require(definition->SpawnInstance(0, &raw_parameters), "Create MIFS parameters");
    return Com<IWbemClassObject>(raw_parameters);
}

void set_packet(IWbemClassObject &parameters, MifsCommand command) {
    const auto packet = encode(command);
    Variant input;
    input.value.vt = VT_ARRAY | VT_UI1;
    input.value.parray = SafeArrayCreateVector(VT_UI1, 0, static_cast<ULONG>(packet.size()));
    if (input.value.parray == nullptr)
        throw Failure{E_OUTOFMEMORY, "Allocate MIFS packet"};
    void *data = nullptr;
    require(SafeArrayAccessData(input.value.parray, &data), "Access MIFS packet");
    std::copy(packet.begin(), packet.end(), static_cast<std::uint8_t *>(data));
    require(SafeArrayUnaccessData(input.value.parray), "Release MIFS packet");
    require(parameters.Put(L"InData", 0, &input.value, CIM_UINT8 | CIM_FLAG_ARRAY), "Set MIFS packet");
}

Com<IWbemClassObject> await_output(IWbemCallResult &call) {
    IWbemClassObject *raw_output = nullptr;
    const auto waited = call.GetResultObject(timeout_ms, &raw_output);
    Com<IWbemClassObject> output(raw_output);
    if (waited == WBEM_S_TIMEDOUT)
        throw Failure{static_cast<HRESULT>(WBEM_E_TIMED_OUT), "MIFS method timeout; outcome unknown"};
    require(waited, "Wait for MIFS output");
    if (!output)
        throw Failure{static_cast<HRESULT>(WBEM_E_FAILED), "MIFS completed without output; outcome unknown"};
    return output;
}

Com<IWbemClassObject> invoke(IWbemServices &services, IWbemClassObject &object,
                             const Com<IWbemClassObject> &input, bool &submitted) {
    Variant path;
    require(object.Get(L"__PATH", 0, &path.value, nullptr, nullptr), "Read MIFS instance path");
    if (path.value.vt != VT_BSTR)
        throw Failure{static_cast<HRESULT>(WBEM_E_TYPE_MISMATCH), "Invalid MIFS instance path"};
    IWbemCallResult *raw_call = nullptr;
    const Text method(L"MiInterface");
    submitted = true;
    const auto invoked = services.ExecMethod(path.value.bstrVal, method.get(), WBEM_FLAG_RETURN_IMMEDIATELY,
                                             nullptr, input.get(), nullptr, &raw_call);
    const Com<IWbemCallResult> call(raw_call);
    require(invoked, "Invoke MIFS method");
    if (!call)
        throw Failure{static_cast<HRESULT>(WBEM_E_FAILED), "MIFS did not return a pending call"};
    return await_output(*call);
}

void copy_output(IWbemClassObject &output, MifsReply &reply) {
    Variant bytes;
    require(output.Get(L"OutData", 0, &bytes.value, nullptr, nullptr), "Read MIFS OutData");
    if (bytes.value.vt != (VT_ARRAY | VT_UI1) || SafeArrayGetDim(bytes.value.parray) != 1)
        throw Failure{static_cast<HRESULT>(WBEM_E_TYPE_MISMATCH), "Invalid MIFS OutData type"};
    long lower = 0;
    long upper = 0;
    require(SafeArrayGetLBound(bytes.value.parray, 1, &lower), "Read MIFS output bound");
    require(SafeArrayGetUBound(bytes.value.parray, 1, &upper), "Read MIFS output bound");
    if (upper - lower + 1 != 30)
        throw Failure{static_cast<HRESULT>(WBEM_E_TYPE_MISMATCH), "Invalid MIFS OutData length"};
    void *data = nullptr;
    require(SafeArrayAccessData(bytes.value.parray, &data), "Access MIFS output");
    std::copy_n(static_cast<const std::uint8_t *>(data), 30, reply.bytes.begin());
    require(SafeArrayUnaccessData(bytes.value.parray), "Release MIFS output");
}

void copy_tail(IWbemClassObject &output, MifsReply &reply) {
    Variant tail;
    require(output.Get(L"ReturnCode", 0, &tail.value, nullptr, nullptr), "Read MIFS ReturnCode");
    Variant converted;
    require(VariantChangeType(&converted.value, &tail.value, 0, VT_UI2), "Validate MIFS ReturnCode");
    reply.bytes[30] = static_cast<std::uint8_t>(converted.value.uiVal & 0xff);
    reply.bytes[31] = static_cast<std::uint8_t>(converted.value.uiVal >> 8);
}

MifsReply perform(MifsCommand command, bool &submitted) {
    const Apartment apartment;
    const auto services = connect();
    const auto active = instance(*services);
    if (!active)
        return {TransportStatus::Unavailable, {}, "No active MIFS instance"};
    const auto input = parameters(*services);
    set_packet(*input, command);
    const auto output = invoke(*services, *active, input, submitted);
    MifsReply reply{TransportStatus::Success, {}, {}};
    copy_output(*output, reply);
    copy_tail(*output, reply);
    return reply;
}

} // namespace

MifsReply NativeWmiTransport::exchange(MifsCommand command) {
    bool submitted = false;
    const bool mutation = command != MifsCommand::ReadCharge && command != MifsCommand::ReadPerformance &&
                          command != MifsCommand::ReadMicrophone;
    try {
        return perform(command, submitted);
    } catch (const Failure &failure) {
        return {native_failure_status(failure.result, submitted && mutation),
                {},
                std::string(failure.stage) + " (HRESULT " + std::to_string(failure.result) + ")"};
    } catch (const std::exception &error) {
        return {native_failure_status(E_FAIL, submitted && mutation), {}, error.what()};
    }
}

} // namespace manager::adapters::windows_firmware
