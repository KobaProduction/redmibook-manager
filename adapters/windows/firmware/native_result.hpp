#pragma once
#include "adapters/windows/firmware/mifs_transport.hpp"
#include <wbemidl.h>
#include <windows.h>
namespace manager::adapters::windows_firmware {
inline TransportStatus native_failure_status(HRESULT result, bool mutation_started) noexcept {
    // Once a SET was dispatched, failure to obtain/parse its result never proves
    // that the firmware write did not happen, including RPC/provider timeouts.
    if (mutation_started)
        return TransportStatus::OutcomeUnknown;
    if (result == E_ACCESSDENIED || result == static_cast<HRESULT>(WBEM_E_ACCESS_DENIED))
        return TransportStatus::PermissionDenied;
    if (result == static_cast<HRESULT>(WBEM_E_INVALID_CLASS) ||
        result == static_cast<HRESULT>(WBEM_E_NOT_FOUND) ||
        result == static_cast<HRESULT>(WBEM_E_INVALID_NAMESPACE))
        return TransportStatus::Unavailable;
    return TransportStatus::Failed;
}
} // namespace manager::adapters::windows_firmware
