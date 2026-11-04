#include "adapters/nfc/ntag.hpp"
#include "adapters/nfc/type5.hpp"
#include "application/nfc/ndef.hpp"
#include <algorithm>
#include <cstdlib>

namespace {
namespace app = manager::application::nfc;
namespace native = manager::adapters::nfc;
class Bus final : public native::Transport {
  public:
    app::Bytes memory = app::Bytes(2048, 0xa5);
    int reads{};
    int writes{};
    int fail_write{};
    bool field{};
    native::Discovery device{app::Status::Ready, true};
    Bus() {
        memory[0] = 0xe1;
        memory[1] = 0x40;
        memory[2] = 0x80;
        memory[3] = 9;
        const auto content = app::make_text_record("Original NFC record with enough room");
        memory[4] = 3;
        memory[5] = static_cast<std::uint8_t>(content.size());
        std::copy(content.begin(), content.end(), memory.begin() + 6);
        memory[6 + content.size()] = 0xfe;
    }
    native::Discovery discover() override {
        return device;
    }
    app::Status read(std::uint16_t block, std::span<std::uint8_t> out) override {
        ++reads;
        const auto offset = static_cast<std::size_t>(block) * 4;
        if (offset + out.size() > memory.size())
            return app::Status::Failed;
        std::copy_n(memory.begin() + static_cast<std::ptrdiff_t>(offset), out.size(), out.begin());
        return app::Status::Ready;
    }
    app::Status write(std::uint16_t block, std::span<const std::uint8_t> bytes) override {
        ++writes;
        if (writes == fail_write)
            return app::Status::Failed;
        if (block >= 511 || bytes.size() != 4)
            return app::Status::InvalidData;
        std::copy(bytes.begin(), bytes.end(), memory.begin() + static_cast<std::ptrdiff_t>(block) * 4);
        return app::Status::Ready;
    }
    app::Status read_register(std::uint16_t, std::uint8_t, std::uint8_t &value) override {
        value = static_cast<std::uint8_t>(field ? 1 : 0);
        return app::Status::Ready;
    }
    bool has_events() const override {
        return false;
    }
    app::Status wait_event(std::stop_token) override {
        return app::Status::Unavailable;
    }
};
bool ndef_boundaries() {
    const auto content = app::make_text_record("Тест NFC");
    const auto records = app::decode_ndef(content);
    if (!records || records->size() != 1 || records->front().type != app::Bytes{'T'})
        return false;
    auto truncated = content;
    truncated.pop_back();
    if (app::decode_ndef(truncated))
        return false;
    if (app::decode_ndef({content.data(), 2}))
        return false;
    auto chunked = content;
    chunked[0] |= 0x20;
    return !app::decode_ndef(chunked) && app::make_text_record(std::string(2100, 'x')).empty();
}
bool type5_preservation() {
    Bus bus;
    const auto original = bus.memory;
    const auto content = app::make_text_record("Short");
    const auto changed = native::replace_type5(original, content);
    if (!changed)
        return false;
    const auto decoded = native::parse_type5(*changed);
    const auto baseline = native::parse_type5(original);
    if (!decoded || !baseline || decoded->content != content)
        return false;
    if (!std::equal(original.begin(), original.begin() + 4, changed->begin()))
        return false;
    const auto restored = native::replace_type5(*changed, baseline->content);
    if (!restored)
        return false;
    return std::equal(original.begin() + 180, original.end(), restored->begin() + 180);
}
bool vendor_without_terminator() {
    Bus bus;
    const auto old = native::parse_type5(bus.memory);
    if (!old)
        return false;
    bus.memory[old->end] = 2;
    const auto parsed = native::parse_type5(bus.memory);
    if (!parsed || parsed->content != old->content)
        return false;
    const auto changed = native::replace_type5(bus.memory, app::make_text_record("Changed"));
    const auto restored = changed ? native::replace_type5(*changed, old->content) : std::nullopt;
    return restored && *restored == bus.memory;
}
bool stale_and_field_barriers() {
    const auto bus = std::make_shared<Bus>();
    native::Ntag tag(bus);
    const auto original = tag.read();
    if (tag.write({{}, app::make_text_record("New")}).status != app::Status::Conflict)
        return false;
    bus->field = true;
    if (tag.write({original.content, app::make_text_record("New")}).status != app::Status::Busy)
        return false;
    return bus->writes == 0 && tag.read().field == true;
}
bool absent_and_read_only() {
    const auto bus = std::make_shared<Bus>();
    native::Ntag tag(bus);
    bus->device = {app::Status::Absent, false};
    if (tag.read().present || bus->reads != 0)
        return false;
    bus->device = {app::Status::Ready, true};
    bus->memory[1] = 0x43;
    const auto state = tag.read();
    return !state.writable &&
           tag.write({state.content, app::make_text_record("New")}).status == app::Status::ReadOnly &&
           bus->writes == 0;
}
bool confirmed_roundtrip() {
    const auto bus = std::make_shared<Bus>();
    native::Ntag tag(bus);
    const auto original = tag.read();
    const auto changed = app::make_text_record("Changed");
    if (tag.write({original.content, changed}).status != app::Status::Ready)
        return false;
    return tag.write({changed, original.content}).status == app::Status::Ready &&
           tag.read().content == original.content;
}
bool stop_after_unknown_write() {
    const auto bus = std::make_shared<Bus>();
    native::Ntag tag(bus);
    const auto original = tag.read();
    bus->fail_write = 2;
    const auto result = tag.write({original.content, app::make_text_record("New")});
    return result.status == app::Status::UnknownOutcome && bus->writes == 2;
}
} // namespace
int main() {
    return ndef_boundaries() && type5_preservation() && vendor_without_terminator() &&
                   stale_and_field_barriers() && absent_and_read_only() && confirmed_roundtrip() &&
                   stop_after_unknown_write()
               ? EXIT_SUCCESS
               : EXIT_FAILURE;
}
