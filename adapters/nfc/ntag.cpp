#include "adapters/nfc/ntag.hpp"
#include "adapters/nfc/type5.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <thread>

namespace manager::adapters::nfc {
namespace {
class Watch final : public application::nfc::Subscription {
  public:
    Watch(std::shared_ptr<Transport> transport, std::function<void()> changed)
        : thread_(
              [transport = std::move(transport), changed = std::move(changed)](const std::stop_token &stop) {
                  if (transport->configure_events(true) != Status::Ready) {
                      transport->configure_events(false);
                      changed();
                      return;
                  }
                  changed();
                  while (!stop.stop_requested()) {
                      const auto status = transport->wait_event(stop);
                      if (stop.stop_requested())
                          break;
                      changed();
                      if (status != Status::Ready)
                          break;
                  }
                  transport->configure_events(false);
              }) {}

  private:
    std::jthread thread_;
};
} // namespace
Ntag::Ntag(std::shared_ptr<Transport> transport) : transport_(std::move(transport)) {}
Status Ntag::read_content() {
    Bytes image;
    const auto status = read_image(image);
    if (status != Status::Ready)
        return status;
    const auto parsed = parse_type5(image);
    if (!parsed)
        return Status::InvalidData;
    state_.capacity = static_cast<std::uint32_t>(parsed->limit - parsed->offset);
    state_.content = parsed->content;
    state_.writable = parsed->writable && state_.field.has_value();
    return Status::Ready;
}
application::nfc::Result Ntag::perform_write(const Bytes &before,
                                             const application::nfc::WriteRequest &request) {
    const auto fresh = parse_type5(before);
    if (!fresh || fresh->content != request.expected)
        return {Status::Conflict, read_unlocked()};
    const auto replacement = replace_type5(before, request.content);
    if (!replacement)
        return {Status::InvalidData, state_};
    update_field();
    if (state_.field != false)
        return {Status::Busy, state_};
    const auto status = commit_image({.before = before, .after = *replacement});
    auto observed = read_unlocked();
    if (status != Status::Ready || observed.status != Status::Ready || observed.content != request.content) {
        observed.writable = false;
        observed.status = Status::UnknownOutcome;
        return {Status::UnknownOutcome, observed};
    }
    return {Status::Ready, std::move(observed)};
}
Status Ntag::read_image(Bytes &image) {
    image.resize(2044); // The final counter block is RF-only; never access it over I2C.
    for (std::size_t offset = 0; offset < image.size(); offset += 64) {
        const auto status = transport_->read(
            static_cast<std::uint16_t>(offset / 4),
            std::span(image).subspan(offset, std::min<std::size_t>(64, image.size() - offset)));
        if (status != Status::Ready)
            return status;
    }
    return Status::Ready;
}
void Ntag::update_field() {
    std::uint8_t value{};
    if (transport_->read_register(0x10a0, 0, value) != Status::Ready) {
        state_.field.reset();
        return;
    }
    const bool field = (value & 1U) != 0;
    if (field && state_.field != true)
        ++state_.encounters;
    state_.field = field;
}
application::nfc::Snapshot Ntag::read_unlocked() {
    const auto device = transport_->discover();
    state_.present = device.present;
    state_.name = device.name;
    state_.event_driven = transport_->events_active();
    state_.status = device.status;
    state_.writable = false;
    if (device.status != Status::Ready)
        return state_;
    update_field();
    state_.status = read_content();
    return state_;
}
application::nfc::Snapshot Ntag::read() {
    const std::lock_guard lock(mutex_);
    return read_unlocked();
}
Status Ntag::write_blocks(ImageChange change) {
    for (std::size_t offset = 0; offset < change.after.size(); offset += 4) {
        const auto block = std::span(change.after).subspan(offset, 4);
        if (std::equal(block.begin(), block.end(),
                       change.before.begin() + static_cast<std::ptrdiff_t>(offset)))
            continue;
        const auto status = transport_->write(static_cast<std::uint16_t>(offset / 4), block);
        if (status != Status::Ready || finish_programming() != Status::Ready)
            return Status::UnknownOutcome;
        std::array<std::uint8_t, 4> observed{};
        if (transport_->read(static_cast<std::uint16_t>(offset / 4), observed) != Status::Ready ||
            !std::equal(block.begin(), block.end(), observed.begin()))
            return Status::UnknownOutcome;
    }
    return Status::Ready;
}
Status Ntag::finish_programming() {
    // NXP NTP53x2: memory NACKs while EEPROM_WR_BUSY is set; register
    // access remains available and no EEPROM-ready interrupt is provided.
    // Only an explicit write uses these bounded, completion-driven I2C status
    // transactions. Never retry the SET, sleep, or infer completion from time.
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(20);
    for (unsigned int transactions = 0; transactions < 64; ++transactions) {
        std::uint8_t value{};
        const auto status = transport_->read_register(0x10a0, 0, value);
        if (status != Status::Ready)
            return status;
        if ((value & 0x40U) != 0)
            return Status::Failed;
        if ((value & 0x80U) == 0)
            return Status::Ready;
        if (std::chrono::steady_clock::now() >= deadline)
            return Status::TimedOut;
    }
    return Status::TimedOut;
}
application::nfc::Result Ntag::write(const application::nfc::WriteRequest &request) {
    const std::lock_guard lock(mutex_);
    const auto current = read_unlocked();
    if (current.status != Status::Ready || !current.writable)
        return {current.status == Status::Ready ? Status::ReadOnly : current.status, current};
    if (current.field == true)
        return {Status::Busy, current};
    if (current.content != request.expected)
        return {Status::Conflict, current};
    Bytes before;
    const auto status = read_image(before);
    return status == Status::Ready ? perform_write(before, request)
                                   : application::nfc::Result{status, current};
}
Status Ntag::commit_image(ImageChange change) {
    const auto parsed = parse_type5(change.before);
    if (!parsed)
        return Status::InvalidData;
    const auto commit_block = (parsed->offset + 1) / 4 * 4;
    auto invalidated = change.before;
    invalidated[parsed->offset + 1] = 0;
    auto status = write_blocks({.before = change.before, .after = invalidated});
    if (status != Status::Ready)
        return status;
    auto staged = change.after;
    std::copy_n(invalidated.begin() + static_cast<std::ptrdiff_t>(commit_block), 4,
                staged.begin() + static_cast<std::ptrdiff_t>(commit_block));
    status = write_blocks({.before = invalidated, .after = staged});
    return status == Status::Ready ? write_blocks({.before = staged, .after = change.after}) : status;
}
std::unique_ptr<application::nfc::Subscription> Ntag::subscribe(std::function<void()> changed) {
    if (!transport_->has_events())
        return {};
    return std::make_unique<Watch>(transport_, std::move(changed));
}
} // namespace manager::adapters::nfc
