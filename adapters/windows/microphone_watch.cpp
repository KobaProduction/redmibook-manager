#include "adapters/platform/microphone_watch.hpp"
#include <array>
#include <atomic>
#include <endpointvolume.h>
#include <mmdeviceapi.h>
#include <mutex>
#include <new>
#include <thread>
#include <utility>
#include <windows.h>

namespace manager::adapters {
namespace {
template <typename T> struct Release {
    void operator()(T *value) const {
        if (value)
            value->Release();
    }
};
template <typename T> using Com = std::unique_ptr<T, Release<T>>;
struct State {
    HANDLE stop{CreateEventW(nullptr, TRUE, FALSE, nullptr)};
    HANDLE wake{CreateEventW(nullptr, FALSE, FALSE, nullptr)};
    std::mutex mutex;
    std::function<void()> changed;
    ~State() {
        if (stop)
            CloseHandle(stop);
        if (wake)
            CloseHandle(wake);
    }
};
void notify(State &state) {
    std::function<void()> changed;
    {
        const std::lock_guard lock(state.mutex);
        changed = state.changed;
    }
    if (changed)
        changed();
}
class Sink final : public IMMNotificationClient, public IAudioEndpointVolumeCallback {
  public:
    explicit Sink(std::shared_ptr<State> state) : state_(std::move(state)) {}
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id, void **result) override {
        if (!result)
            return E_POINTER;
        *result = nullptr;
        if (id == IID_IUnknown || id == __uuidof(IMMNotificationClient))
            *result = static_cast<IMMNotificationClient *>(this);
        else if (id == __uuidof(IAudioEndpointVolumeCallback))
            *result = static_cast<IAudioEndpointVolumeCallback *>(this);
        else
            return E_NOINTERFACE;
        AddRef();
        return S_OK;
    }
    ULONG STDMETHODCALLTYPE AddRef() override {
        return ++references_;
    }
    ULONG STDMETHODCALLTYPE Release() override {
        const auto count = --references_;
        if (!count)
            delete this;
        return count;
    }
    HRESULT STDMETHODCALLTYPE OnNotify(PAUDIO_VOLUME_NOTIFICATION_DATA) override {
        return wake();
    }
    HRESULT STDMETHODCALLTYPE OnDefaultDeviceChanged(EDataFlow flow, ERole role, LPCWSTR) override {
        return flow == eCapture && role == eCommunications ? wake() : S_OK;
    }
    HRESULT STDMETHODCALLTYPE OnDeviceAdded(LPCWSTR) override {
        return wake();
    }
    HRESULT STDMETHODCALLTYPE OnDeviceRemoved(LPCWSTR) override {
        return wake();
    }
    HRESULT STDMETHODCALLTYPE OnDeviceStateChanged(LPCWSTR, DWORD) override {
        return wake();
    }
    HRESULT STDMETHODCALLTYPE OnPropertyValueChanged(LPCWSTR, const PROPERTYKEY) override {
        return S_OK;
    }

  private:
    HRESULT wake() {
        SetEvent(state_->wake);
        return S_OK;
    }
    std::atomic<ULONG> references_{1};
    std::shared_ptr<State> state_;
};
Com<IAudioEndpointVolume> volume(IMMDeviceEnumerator &devices) {
    IMMDevice *raw_device = nullptr;
    if (FAILED(devices.GetDefaultAudioEndpoint(eCapture, eCommunications, &raw_device)))
        return {};
    const Com<IMMDevice> device(raw_device);
    IAudioEndpointVolume *raw_volume = nullptr;
    if (FAILED(device->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_ALL, nullptr,
                                reinterpret_cast<void **>(&raw_volume))))
        return {};
    return Com<IAudioEndpointVolume>(raw_volume);
}
void listen(State &state, IMMDeviceEnumerator &devices, Sink &sink) {
    Com<IAudioEndpointVolume> endpoint;
    const std::array<HANDLE, 2> events{state.stop, state.wake};
    SetEvent(state.wake);
    while (WaitForMultipleObjects(2, events.data(), FALSE, INFINITE) == WAIT_OBJECT_0 + 1) {
        if (endpoint)
            endpoint->UnregisterControlChangeNotify(&sink);
        endpoint = volume(devices);
        if (endpoint && FAILED(endpoint->RegisterControlChangeNotify(&sink)))
            endpoint.reset();
        notify(state);
    }
    if (endpoint)
        endpoint->UnregisterControlChangeNotify(&sink);
}
void run(const std::shared_ptr<State> &state) noexcept {
    const auto apartment = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(apartment))
        return;
    IMMDeviceEnumerator *raw = nullptr;
    const auto created = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                                          __uuidof(IMMDeviceEnumerator), reinterpret_cast<void **>(&raw));
    {
        const Com<IMMDeviceEnumerator> devices(raw);
        const Com<Sink> sink(new (std::nothrow) Sink(state));
        if (SUCCEEDED(created) && sink &&
            SUCCEEDED(devices->RegisterEndpointNotificationCallback(sink.get()))) {
            try {
                listen(*state, *devices, *sink);
            } catch (...) {
                SetEvent(state->stop);
            }
            devices->UnregisterEndpointNotificationCallback(sink.get());
        }
    }
    CoUninitialize();
}
class Watch final : public application::microphone::ChangeSubscription {
  public:
    explicit Watch(std::function<void()> changed) : state_(std::make_shared<State>()) {
        state_->changed = std::move(changed);
        if (!state_->stop || !state_->wake)
            return;
        std::thread([state = state_] { run(state); }).detach();
    }
    ~Watch() override {
        {
            const std::lock_guard lock(state_->mutex);
            state_->changed = {};
        }
        if (state_->stop)
            SetEvent(state_->stop);
    }

  private:
    std::shared_ptr<State> state_;
};
} // namespace
std::unique_ptr<application::microphone::ChangeSubscription>
watch_native_microphone(std::function<void()> changed) {
    return std::make_unique<Watch>(std::move(changed));
}
} // namespace manager::adapters
