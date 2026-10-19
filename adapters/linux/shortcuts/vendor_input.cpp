#include "adapters/platform/vendor_input.hpp"

namespace manager::adapters {
namespace {
class UnimplementedVendorInput final : public application::shortcuts::VendorInputPort {
  public:
    application::shortcuts::VendorInputStatus start(NotificationCallback, StatusCallback) override {
        return {application::shortcuts::VendorInputState::NotImplemented,
                "Linux vendor input backend is not implemented"};
    }
    std::shared_future<void> stop() override {
        std::promise<void> complete;
        complete.set_value();
        return complete.get_future().share();
    }
};
} // namespace

std::unique_ptr<application::shortcuts::VendorInputPort>
make_native_vendor_input(const application::DeviceProfile &) {
    return std::make_unique<UnimplementedVendorInput>();
}
} // namespace manager::adapters
