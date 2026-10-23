#include "adapters/platform/power_scheme.hpp"
namespace manager::adapters {
namespace {
class Schemes final : public application::power::SchemePort {
  public:
    application::power::Snapshot read() const override {
        return {application::power::Status::NotImplemented, {}, {}};
    }
    application::power::Status select(const std::string &) override {
        return application::power::Status::NotImplemented;
    }
};
} // namespace
std::shared_ptr<application::power::SchemePort> make_power_schemes() {
    return std::make_shared<Schemes>();
}
} // namespace manager::adapters
