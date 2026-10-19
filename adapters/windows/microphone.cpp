#include "adapters/platform/session_controls.hpp"
#include <endpointvolume.h>
#include <memory>
#include <mmdeviceapi.h>
#include <windows.h>

namespace manager::adapters {
namespace {
template <typename T> struct Release {
    void operator()(T *value) const {
        if (value) {
            value->Release();
        }
    }
};
template <typename T> using ComPtr = std::unique_ptr<T, Release<T>>;
class Apartment {
  public:
    Apartment() : status_(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED)) {}
    ~Apartment() {
        if (SUCCEEDED(status_)) {
            CoUninitialize();
        }
    }
    bool ready() const {
        return SUCCEEDED(status_) || status_ == RPC_E_CHANGED_MODE;
    }

  private:
    HRESULT status_;
};
ComPtr<IAudioEndpointVolume> endpoint() {
    IMMDeviceEnumerator *raw_enumerator{};
    if (FAILED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_INPROC_SERVER,
                                __uuidof(IMMDeviceEnumerator), reinterpret_cast<void **>(&raw_enumerator)))) {
        return {};
    }
    const ComPtr<IMMDeviceEnumerator> enumerator(raw_enumerator);
    IMMDevice *raw_device{};
    if (FAILED(enumerator->GetDefaultAudioEndpoint(eCapture, eCommunications, &raw_device))) {
        return {};
    }
    const ComPtr<IMMDevice> device(raw_device);
    IAudioEndpointVolume *raw_volume{};
    if (FAILED(device->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_INPROC_SERVER, nullptr,
                                reinterpret_cast<void **>(&raw_volume)))) {
        return {};
    }
    return ComPtr<IAudioEndpointVolume>(raw_volume);
}
application::microphone::Observation update_endpoint(IAudioEndpointVolume &volume,
                                                     std::optional<bool> requested) {
    using application::microphone::Status;
    if (requested) {
        const HRESULT result = volume.SetMute(*requested ? TRUE : FALSE, nullptr);
        if (FAILED(result)) {
            return {result == E_ACCESSDENIED ? Status::Denied : Status::Failed, {}, "microphone-set-failed"};
        }
    }
    BOOL muted{};
    if (FAILED(volume.GetMute(&muted))) {
        return {Status::Failed, {}, "microphone-read-failed"};
    }
    if (requested && (muted != FALSE) != *requested) {
        return {Status::Failed, muted != FALSE, "microphone-readback-mismatch"};
    }
    return {Status::Ready, muted != FALSE, "default-communications-endpoint"};
}

application::microphone::Observation operate(std::optional<bool> requested) {
    using application::microphone::Status;
    const Apartment apartment;
    if (!apartment.ready()) {
        return {Status::Failed, {}, "audio-com-initialization-failed"};
    }
    const auto volume = endpoint();
    if (!volume) {
        return {Status::Unavailable, {}, "default-communications-microphone-unavailable"};
    }
    auto observation = update_endpoint(*volume, requested);
    DWORD support{};
    if (SUCCEEDED(volume->QueryHardwareSupport(&support))) {
        observation.hardware_mute_supported = (support & ENDPOINT_HARDWARE_SUPPORT_MUTE) != 0;
    }
    return observation;
}
} // namespace
application::microphone::Observation NativeMicrophone::read() const {
    return operate({});
}
application::microphone::Observation NativeMicrophone::set_muted(bool muted) const {
    return operate(muted);
}
} // namespace manager::adapters
