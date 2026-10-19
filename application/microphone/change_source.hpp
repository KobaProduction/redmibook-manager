#pragma once
#include <memory>

namespace manager::application::microphone {
class ChangeSubscription {
  public:
    virtual ~ChangeSubscription() = default;
    // Destruction stops new deliveries without joining a native worker. A delivery
    // already reserved can finish; its consumer must guard its target lifetime.
};
} // namespace manager::application::microphone
