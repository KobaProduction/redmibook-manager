#include "adapters/nfc/ntag.hpp"
#include "adapters/platform/nfc.hpp"
#include "adapters/windows/nfc/io.hpp"
#include <algorithm>
#include <array>
#include <mutex>

namespace manager::adapters::nfc {
namespace {
constexpr DWORD open_controller = 0x04000400;
constexpr DWORD write_read = 0x04000410;
constexpr DWORD wait_interrupt = 0x04000420;
class WindowsTransport final : public Transport {
  public:
    Discovery discover() override;
    Status read(std::uint16_t block, std::span<std::uint8_t> result) override;
    Status write(std::uint16_t block, std::span<const std::uint8_t> bytes) override;
    Status read_register(std::uint16_t block, std::uint8_t index, std::uint8_t &value) override;
    bool has_events() const override {
        return handle_ != nullptr;
    }
    Status wait_event(std::stop_token stop) override {
        return windows::status(
            windows::transfer(handle_, wait_interrupt, {}, 0, event_pending_, INFINITE, stop).error);
    }
    Status configure_events(bool enable) override;
    bool events_active() const override {
        return events_active_.load();
    }

  private:
    std::atomic<bool> events_active_{false};
    std::mutex io_;
    std::optional<std::uint8_t> original_event_mode_;
    std::shared_ptr<windows::Handle> handle_;
    std::shared_ptr<std::atomic<bool>> pending_{std::make_shared<std::atomic<bool>>(false)};
    std::shared_ptr<std::atomic<bool>> event_pending_{std::make_shared<std::atomic<bool>>(false)};
    Status exchange(Bytes command, std::span<std::uint8_t> output);
    Status set_event_mode(std::uint8_t mode);
};
Status WindowsTransport::set_event_mode(std::uint8_t mode) {
    Status status{};
    {
        const std::lock_guard lock(io_);
        status =
            windows::status(windows::transfer(handle_, 0, {0x10, 0xa8, 0, 0x0f, mode}, 0, pending_).error);
    }
    if (status != Status::Ready)
        return status;
    std::uint8_t observed{};
    status = read_register(0x10a8, 0, observed);
    return status == Status::Ready && (observed & 0x0fU) == mode ? Status::Ready : Status::UnknownOutcome;
}
Discovery WindowsTransport::discover() {
    if (handle_)
        return {*pending_ ? Status::Busy : Status::Ready, true};
    if (!windows::present())
        return {};
    if (!windows::supported_driver())
        return {Status::Incompatible, true};
    const auto device = CreateFileW(L"\\\\.\\SPBNFC01", GENERIC_READ | GENERIC_WRITE, 0, nullptr,
                                    OPEN_EXISTING, FILE_FLAG_OVERLAPPED, nullptr);
    if (device == INVALID_HANDLE_VALUE)
        return {windows::status(GetLastError()), true};
    auto handle = std::make_shared<windows::Handle>(device);
    const auto opened = windows::transfer(handle, open_controller, {}, 0, pending_);
    if (opened.error != ERROR_SUCCESS)
        return {windows::status(opened.error), true};
    handle_ = std::move(handle);
    return {Status::Ready, true};
}
Status WindowsTransport::exchange(Bytes command, std::span<std::uint8_t> output) {
    const std::lock_guard lock(io_);
    const auto reply = windows::transfer(handle_, write_read, std::move(command), output.size(), pending_);
    if (reply.error != ERROR_SUCCESS)
        return windows::status(reply.error);
    if (reply.bytes.size() != output.size())
        return Status::Failed;
    std::copy(reply.bytes.begin(), reply.bytes.end(), output.begin());
    return Status::Ready;
}
Status WindowsTransport::read(std::uint16_t block, std::span<std::uint8_t> result) {
    return exchange({static_cast<std::uint8_t>(block >> 8U), static_cast<std::uint8_t>(block & 0xffU)},
                    result);
}
Status WindowsTransport::read_register(std::uint16_t block, std::uint8_t index, std::uint8_t &value) {
    return exchange({static_cast<std::uint8_t>(block >> 8U), static_cast<std::uint8_t>(block & 0xffU), index},
                    std::span(&value, 1));
}
Status WindowsTransport::write(std::uint16_t block, std::span<const std::uint8_t> bytes) {
    const std::lock_guard lock(io_);
    if (block >= 511 || bytes.size() != 4)
        return Status::InvalidData;
    Bytes command{static_cast<std::uint8_t>(block >> 8U), static_cast<std::uint8_t>(block & 0xffU)};
    command.insert(command.end(), bytes.begin(), bytes.end());
    return windows::status(windows::transfer(handle_, 0, std::move(command), 0, pending_).error);
}
Status WindowsTransport::configure_events(bool enable) {
    if (!enable)
        events_active_ = false;
    std::uint8_t current{};
    auto status = read_register(0x10a8, 0, current);
    if (status != Status::Ready)
        return status;
    if (enable)
        original_event_mode_ = current & 0x0fU;
    else if (!original_event_mode_ || (current & 0x0fU) != 1)
        return Status::Ready;
    const auto mode = enable ? std::uint8_t{1} : *original_event_mode_;
    status = set_event_mode(mode);
    events_active_ = status == Status::Ready && enable;
    return status;
}
} // namespace
} // namespace manager::adapters::nfc
namespace manager::adapters {
std::shared_ptr<application::nfc::TagPort> make_nfc_tag() {
    return std::make_shared<nfc::Ntag>(std::make_shared<nfc::WindowsTransport>());
}
} // namespace manager::adapters
