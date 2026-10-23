#include "adapters/windows/battery/policy_store.hpp"
#include "adapters/windows/firmware/native_result.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <windows.h>

namespace {
namespace fw = manager::adapters::windows_firmware;
bool native_results() {
    if (fw::native_failure_status(static_cast<HRESULT>(WBEM_E_TIMED_OUT), true) !=
            fw::TransportStatus::OutcomeUnknown ||
        fw::native_failure_status(E_FAIL, true) != fw::TransportStatus::OutcomeUnknown ||
        fw::native_failure_status(E_ACCESSDENIED, false) != fw::TransportStatus::PermissionDenied)
        return false;
    bool latched = false;
    const auto timeout = fw::native_failure_status(static_cast<HRESULT>(WBEM_E_TIMED_OUT), true);
    const auto result = fw::observe_reply(fw::MifsCommand::Turbo, {timeout, {}, "timeout"}, latched);
    if (result.status != fw::TransportStatus::OutcomeUnknown || !latched)
        return false;
    latched = false;
    const auto malformed =
        fw::observe_reply(fw::MifsCommand::Quiet, {fw::TransportStatus::Success, {}, {}}, latched);
    if (malformed.status != fw::TransportStatus::OutcomeUnknown || !latched)
        return false;
    latched = false;
    static_cast<void>(fw::observe_reply(fw::MifsCommand::ReadPerformance, {timeout, {}, {}}, latched));
    return !latched;
}
bool pending_policy() {
    const auto root = std::filesystem::temp_directory_path() /
                      ("redmibook-policy-test-" + std::to_string(GetCurrentProcessId()));
    if (!std::filesystem::create_directory(root))
        return false;
    struct Cleanup {
        std::filesystem::path path;
        ~Cleanup() {
            std::error_code error;
            std::filesystem::remove_all(path, error);
        }
    } cleanup{root};
    std::ofstream(root / "charge-write.pending") << "";
    std::ofstream(root / "charge-policy.v1") << "invalid";
    const auto invalid = manager::adapters::windows_battery::load_policy(root);
    std::filesystem::remove(root / "charge-policy.v1");
    const auto missing = manager::adapters::windows_battery::load_policy(root);
    return !invalid.valid && invalid.unsettled && !invalid.desired && missing.valid && missing.unsettled;
}
} // namespace
int main() {
    const bool passed = native_results() && pending_policy();
    if (!passed)
        std::cerr << "Firmware outcome/pending policy safety contract failed\n";
    return passed ? 0 : 1;
}
