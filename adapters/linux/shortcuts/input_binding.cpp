#include "adapters/platform/input_binding.hpp"

namespace manager::adapters {
namespace {
class UnimplementedInput final : public application::shortcuts::InputPort {
  public:
    application::shortcuts::BindingResult bind(application::shortcuts::KeyChord,
                                               std::function<void()>) override {
        return {application::shortcuts::BindingStatus::NotImplemented, "linux-input-not-implemented"};
    }
    void unbind() override {}
};
} // namespace
std::unique_ptr<application::shortcuts::InputPort> make_native_input() {
    return std::make_unique<UnimplementedInput>();
}
} // namespace manager::adapters
