#include "adapters/platform/vendor_input.hpp"
#include "adapters/windows/shortcuts/vendor_decoder.hpp"

#include <windows.h>

#include <oleauto.h>
#include <wbemidl.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <deque>
#include <exception>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <thread>
#include <utility>
#include <vector>

namespace manager::adapters {
namespace {
using Port = application::shortcuts::VendorInputPort;
using Status = application::shortcuts::VendorInputStatus;
using InputState = application::shortcuts::VendorInputState;
using Clock = std::chrono::steady_clock;
constexpr std::size_t queue_limit = 128;

struct Failure {
    HRESULT result;
    const char *stage;
};

void require(HRESULT result, const char *stage) {
    if (FAILED(result)) {
        throw Failure{result, stage};
    }
}

template <typename T> struct Release {
    void operator()(T *object) const noexcept {
        if (object != nullptr) {
            object->Release();
        }
    }
};
template <typename T> using Com = std::unique_ptr<T, Release<T>>;

class Event final {
  public:
    Event() : value_(CreateEventW(nullptr, TRUE, FALSE, nullptr)) {
        if (value_ == nullptr) {
            throw Failure{HRESULT_FROM_WIN32(GetLastError()), "Create vendor-input wake event"};
        }
    }
    ~Event() {
        CloseHandle(value_);
    }
    Event(const Event &) = delete;
    Event &operator=(const Event &) = delete;
    [[nodiscard]] HANDLE get() const {
        return value_;
    }

  private:
    HANDLE value_;
};

class Apartment final {
  public:
    Apartment() {
        require(CoInitializeEx(nullptr, COINIT_MULTITHREADED), "Initialize vendor-input MTA");
    }
    ~Apartment() {
        CoUninitialize();
    }
    Apartment(const Apartment &) = delete;
    Apartment &operator=(const Apartment &) = delete;
};

class Text final {
  public:
    explicit Text(const wchar_t *text) : value_(SysAllocString(text)) {
        if (value_ == nullptr) {
            throw Failure{E_OUTOFMEMORY, "Allocate vendor-input WMI query"};
        }
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

class Variant final {
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

struct Callbacks {
    Port::NotificationCallback notification;
    Port::StatusCallback status;
};

struct Packet {
    std::vector<std::uint8_t> bytes;
    std::chrono::milliseconds observed_at;
};

struct Batch {
    std::deque<Packet> packets;
    bool overflow{false};
    std::optional<HRESULT> completion;
};

struct State {
    Event stop;
    Event wake;
    std::mutex mutex;
    bool stopping{false};
    std::shared_ptr<const Callbacks> callbacks;
    Batch incoming;
    std::promise<void> finished;
    std::shared_future<void> completion{finished.get_future().share()};
    std::optional<HRESULT> cancel_failure;
};

std::shared_ptr<const Callbacks> reserve_callbacks(State &state) {
    const std::lock_guard guard(state.mutex);
    return state.stopping ? nullptr : state.callbacks;
}

void request_stop(State &state);

void report(State &state, Status status) noexcept {
    try {
        const auto callbacks = reserve_callbacks(state);
        if (callbacks && callbacks->status) {
            callbacks->status(std::move(status));
        }
    } catch (...) {
        // A throwing consumer invalidates the stream instead of silently losing status.
        request_stop(state);
    }
}

void request_stop(State &state) {
    std::shared_ptr<const Callbacks> released;
    {
        const std::lock_guard guard(state.mutex);
        state.stopping = true;
        released = std::exchange(state.callbacks, nullptr);
        SetEvent(state.stop.get());
    }
}

void complete_backend(State &state, HRESULT result) {
    const std::lock_guard guard(state.mutex);
    if (!state.stopping) {
        state.incoming.completion = result;
        SetEvent(state.wake.get());
    }
}

void overflow(State &state) {
    const std::lock_guard guard(state.mutex);
    if (!state.stopping) {
        state.incoming.overflow = true;
        SetEvent(state.wake.get());
    }
}

void enqueue(State &state, std::vector<std::uint8_t> bytes) {
    const std::lock_guard guard(state.mutex);
    if (state.stopping) {
        return;
    }
    if (state.incoming.packets.size() == queue_limit) {
        state.incoming.overflow = true;
    } else {
        const auto time =
            std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now().time_since_epoch());
        state.incoming.packets.push_back({std::move(bytes), time});
    }
    SetEvent(state.wake.get());
}

std::vector<std::uint8_t> packet_bytes(IWbemClassObject &object) {
    Variant detail;
    require(object.Get(L"EventDetail", 0, &detail.value, nullptr, nullptr), "Read vendor-input EventDetail");
    if (detail.value.vt != (VT_ARRAY | VT_UI1) || detail.value.parray == nullptr ||
        SafeArrayGetDim(detail.value.parray) != 1) {
        throw Failure{static_cast<HRESULT>(WBEM_E_TYPE_MISMATCH), "Invalid vendor-input payload type"};
    }
    long lower = 0;
    long upper = 0;
    require(SafeArrayGetLBound(detail.value.parray, 1, &lower), "Read vendor-input array lower bound");
    require(SafeArrayGetUBound(detail.value.parray, 1, &upper), "Read vendor-input array upper bound");
    const auto length = static_cast<long long>(upper) - static_cast<long long>(lower) + 1;
    if (length < 1 || length > 64) {
        throw Failure{static_cast<HRESULT>(WBEM_E_TYPE_MISMATCH), "Vendor-input payload exceeds 64 bytes"};
    }
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(length));
    void *data = nullptr;
    require(SafeArrayAccessData(detail.value.parray, &data), "Access vendor-input payload");
    std::copy_n(static_cast<const std::uint8_t *>(data), bytes.size(), bytes.begin());
    require(SafeArrayUnaccessData(detail.value.parray), "Release vendor-input payload");
    return bytes;
}

class Sink final : public IWbemObjectSink {
  public:
    explicit Sink(std::shared_ptr<State> state) : state_(std::move(state)) {}
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id, void **object) override {
        if (object == nullptr) {
            return E_POINTER;
        }
        *object = nullptr;
        if (id != IID_IUnknown && id != IID_IWbemObjectSink) {
            return E_NOINTERFACE;
        }
        *object = static_cast<IWbemObjectSink *>(this);
        AddRef();
        return S_OK;
    }
    ULONG STDMETHODCALLTYPE AddRef() override {
        return references_.fetch_add(1) + 1;
    }
    ULONG STDMETHODCALLTYPE Release() override {
        const auto remaining = references_.fetch_sub(1) - 1;
        if (remaining == 0) {
            delete this;
        }
        return remaining;
    }
    HRESULT STDMETHODCALLTYPE Indicate(LONG count, IWbemClassObject **objects) override {
        try {
            receive(count, objects);
            return WBEM_S_NO_ERROR;
        } catch (const Failure &failure) {
            complete_backend(*state_, failure.result);
            return WBEM_S_NO_ERROR;
        } catch (...) {
            complete_backend(*state_, E_OUTOFMEMORY);
            return WBEM_S_NO_ERROR;
        }
    }
    HRESULT STDMETHODCALLTYPE SetStatus(LONG flags, HRESULT result, BSTR, IWbemClassObject *) override {
        const auto completion = flags == WBEM_STATUS_COMPLETE ? std::optional<HRESULT>{result} : std::nullopt;
        if (completion) {
            complete_backend(*state_, *completion);
        }
        return WBEM_S_NO_ERROR;
    }

  private:
    void receive(LONG count, IWbemClassObject **objects) {
        if (count < 0 || (count > 0 && objects == nullptr)) {
            throw Failure{E_INVALIDARG, "Invalid vendor-input callback batch"};
        }
        if (count > static_cast<LONG>(queue_limit)) {
            overflow(*state_);
            return;
        }
        for (LONG index = 0; index < count; ++index) {
            if (objects[index] == nullptr) {
                throw Failure{E_INVALIDARG, "Missing vendor-input event"};
            }
            enqueue(*state_, packet_bytes(*objects[index]));
        }
    }
    std::atomic<ULONG> references_{1};
    std::shared_ptr<State> state_;
};

Com<IWbemServices> connect() {
    IWbemLocator *raw_locator = nullptr;
    require(CoCreateInstance(CLSID_WbemLocator, nullptr, CLSCTX_INPROC_SERVER, IID_IWbemLocator,
                             reinterpret_cast<void **>(&raw_locator)),
            "Create vendor-input WMI locator");
    const Com<IWbemLocator> locator(raw_locator);
    IWbemServices *raw_services = nullptr;
    const Text space(L"ROOT\\WMI");
    const auto result =
        locator->ConnectServer(space.get(), nullptr, nullptr, nullptr, WBEM_FLAG_CONNECT_USE_MAX_WAIT,
                               nullptr, nullptr, &raw_services);
    Com<IWbemServices> services(raw_services);
    require(result, "Connect vendor-input ROOT/WMI");
    require(CoSetProxyBlanket(services.get(), RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, nullptr,
                              RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, EOAC_NONE),
            "Set vendor-input WMI identity");
    return services;
}

class Subscription final {
  public:
    explicit Subscription(std::shared_ptr<State> state)
        : state_(std::move(state)), services_(connect()), sink_(new Sink(state_)) {
        const Text language(L"WQL");
        const Text query(L"SELECT * FROM HID_EVENT20");
        require(services_->ExecNotificationQueryAsync(language.get(), query.get(), 0, nullptr, sink_.get()),
                "Subscribe asynchronous HID_EVENT20");
    }
    ~Subscription() {
        const auto result = services_->CancelAsyncCall(sink_.get());
        if (FAILED(result)) {
            state_->cancel_failure = result;
        }
    }
    Subscription(const Subscription &) = delete;
    Subscription &operator=(const Subscription &) = delete;

  private:
    std::shared_ptr<State> state_;
    Com<IWbemServices> services_;
    Com<Sink> sink_;
};

Status failure_status(const Failure &failure) {
    auto state = InputState::Failed;
    if (failure.result == E_ACCESSDENIED || failure.result == static_cast<HRESULT>(WBEM_E_ACCESS_DENIED)) {
        state = InputState::PermissionDenied;
    } else if (failure.result == static_cast<HRESULT>(WBEM_E_INVALID_CLASS) ||
               failure.result == static_cast<HRESULT>(WBEM_E_INVALID_NAMESPACE)) {
        state = InputState::BackendUnavailable;
    }
    return {state, std::string(failure.stage) + " (HRESULT " + std::to_string(failure.result) + ")"};
}

Batch take_batch(State &state) {
    const std::lock_guard guard(state.mutex);
    Batch batch;
    std::swap(batch, state.incoming);
    ResetEvent(state.wake.get());
    return batch;
}

void dispatch(State &state, windows_shortcuts::VendorEventDecoder &decoder, Batch batch) {
    if (batch.completion) {
        const auto result =
            FAILED(*batch.completion) ? *batch.completion : static_cast<HRESULT>(WBEM_E_FAILED);
        throw Failure{result, "Vendor-input subscription ended"};
    }
    if (batch.overflow) {
        decoder.reset();
        report(state, {InputState::Overflow, "Vendor-input queue overflow; partial sequences discarded"});
        return;
    }
    for (const auto &packet : batch.packets) {
        const auto notification = decoder.consume(packet.bytes, packet.observed_at);
        const auto callbacks = reserve_callbacks(state);
        if (notification && callbacks && callbacks->notification) {
            callbacks->notification(*notification);
        }
    }
}

void listen(State &state, windows_shortcuts::VendorEventDecoder &decoder) {
    const std::array<HANDLE, 2> events{state.stop.get(), state.wake.get()};
    while (true) {
        const auto event =
            WaitForMultipleObjects(static_cast<DWORD>(events.size()), events.data(), FALSE, INFINITE);
        if (event == WAIT_OBJECT_0) {
            return;
        }
        if (event != WAIT_OBJECT_0 + 1) {
            throw Failure{HRESULT_FROM_WIN32(GetLastError()), "Wait for vendor-input event"};
        }
        dispatch(state, decoder, take_batch(state));
    }
}

void finish(State &state) {
    request_stop(state);
    if (state.cancel_failure) {
        state.finished.set_exception(std::make_exception_ptr(std::runtime_error(
            "Vendor-input cancellation failed: " + std::to_string(*state.cancel_failure))));
    } else {
        state.finished.set_value();
    }
}

void run(const std::shared_ptr<State> &state, const application::DeviceProfile &profile) noexcept {
    try {
        windows_shortcuts::VendorEventDecoder decoder(profile);
        if (!decoder.supported()) {
            report(*state,
                   {InputState::Unsupported, "Vendor-input decoder is not qualified for this device"});
        } else if (WaitForSingleObject(state->stop.get(), 0) != WAIT_OBJECT_0) {
            const Apartment apartment;
            const Subscription subscription(state);
            report(*state, {InputState::Ready, "Asynchronous HID_EVENT20 subscription active"});
            listen(*state, decoder);
        }
    } catch (const Failure &failure) {
        report(*state, failure_status(failure));
    } catch (const std::exception &failure) {
        report(*state, {InputState::Failed, failure.what()});
    } catch (...) {
        report(*state, {InputState::Failed, "Unexpected vendor-input worker failure"});
    }
    finish(*state);
}

class NativeVendorInput final : public Port {
  public:
    explicit NativeVendorInput(application::DeviceProfile profile) : profile_(std::move(profile)) {}
    ~NativeVendorInput() override {
        static_cast<void>(stop());
    }
    Status start(NotificationCallback notification, StatusCallback status) override {
        const std::lock_guard guard(mutex_);
        if (state_) {
            return {InputState::Failed, "Vendor-input source can only be started once"};
        }
        try {
            state_ = std::make_shared<State>();
            state_->callbacks =
                std::make_shared<Callbacks>(Callbacks{std::move(notification), std::move(status)});
            std::thread([state = state_, profile = profile_] { run(state, profile); }).detach();
            return {InputState::Starting, "Starting vendor-input subscription"};
        } catch (const Failure &failure) {
            abandon_start();
            return failure_status(failure);
        } catch (const std::exception &failure) {
            abandon_start();
            return {InputState::Failed, failure.what()};
        }
    }
    std::shared_future<void> stop() override {
        std::shared_ptr<State> state;
        {
            const std::lock_guard guard(mutex_);
            state = state_;
        }
        if (state) {
            request_stop(*state);
            return state->completion;
        }
        std::promise<void> complete;
        complete.set_value();
        return complete.get_future().share();
    }

  private:
    void abandon_start() {
        if (state_) {
            request_stop(*state_);
            state_->finished.set_value();
        }
    }
    std::mutex mutex_;
    application::DeviceProfile profile_;
    std::shared_ptr<State> state_;
};
} // namespace

std::unique_ptr<application::shortcuts::VendorInputPort>
make_native_vendor_input(const application::DeviceProfile &profile) {
    return std::make_unique<NativeVendorInput>(profile);
}
} // namespace manager::adapters
