#pragma once

#include "application/shortcuts/vendor_activation.hpp"
#include <cstdint>
#include <functional>
#include <future>
#include <string>

namespace manager::application::shortcuts {
enum class VendorInputState : std::uint8_t {
    Starting,
    Ready,
    Unsupported,
    NotImplemented,
    PermissionDenied,
    BackendUnavailable,
    Overflow,
    Failed,
    Stopped
};

struct VendorInputStatus {
    VendorInputState state{VendorInputState::Stopped};
    std::string diagnostic;
};

class VendorInputPort {
  public:
    using NotificationCallback = std::function<void(VendorNotification)>;
    using StatusCallback = std::function<void(VendorInputStatus)>;
    virtual ~VendorInputPort() = default;
    // One start per source. Returns promptly; Starting is not proof of subscription.
    // Readiness/failure and notifications are serialized on the source's worker.
    // Callbacks must not throw or wait for stop completion on that same worker.
    // The bounded source queue may overflow: partial sequences are discarded/reset,
    // and Overflow is reported rather than replaying uncertain activations.
    virtual VendorInputStatus start(NotificationCallback notification, StatusCallback status) = 0;
    // Invalidates new callback reservations synchronously and never joins the worker.
    // A callback reserved before stop may still finish. The shared future completes
    // only after callback dispatch and backend cancellation/cleanup have finished.
    // A caller deadline does not prove cancellation; keep callback targets guarded.
    // The future carries an exception if native cancellation could not be confirmed.
    // Destruction requests stop without waiting; worker/backend retain their lifetime.
    [[nodiscard]] virtual std::shared_future<void> stop() = 0;
};
} // namespace manager::application::shortcuts
