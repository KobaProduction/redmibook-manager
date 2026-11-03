#include "adapters/nfc/type5.hpp"
#include "application/nfc/ndef.hpp"
#include <algorithm>

namespace manager::adapters::nfc {
using application::nfc::Bytes;
namespace {
struct Tlv {
    std::size_t offset{}, value{}, length{}, end{};
    std::uint8_t type{};
};
std::optional<Tlv> sized_tlv(std::span<const std::uint8_t> memory, Tlv value) {
    if (value.value >= memory.size())
        return {};
    value.length = memory[value.value++];
    if (value.length == 0xff) {
        if (memory.size() - value.value < 2)
            return {};
        value.length = (static_cast<std::size_t>(memory[value.value]) << 8U) | memory[value.value + 1];
        value.value += 2;
    }
    if (value.length > memory.size() - value.value)
        return {};
    value.end = value.value + value.length;
    return value;
}
Type5Image extract_ndef(std::span<const std::uint8_t> memory, const Tlv &tlv, Type5Image result) {
    result.offset = tlv.offset;
    result.end = tlv.end;
    result.used = tlv.end;
    result.content.assign(memory.begin() + static_cast<std::ptrdiff_t>(tlv.value),
                          memory.begin() + static_cast<std::ptrdiff_t>(tlv.end));
    // NXP specifies undefined random EEPROM after the NDEF allocation.
    // Preserve it; include only an immediately adjacent terminator.
    if (result.end < result.limit && memory[result.end] == 0xfe)
        ++result.used;
    return result;
}
std::optional<Tlv> next_tlv(std::span<const std::uint8_t> memory, std::size_t offset) {
    if (offset >= memory.size())
        return {};
    Tlv value{offset, offset + 1, 0, offset + 1, memory[offset]};
    if (value.type == 0 || value.type == 0xfe)
        return value;
    return sized_tlv(memory, value);
}
std::optional<std::pair<std::size_t, std::size_t>> bounds(std::span<const std::uint8_t> memory) {
    if (memory.size() < 8 || (memory[0] != 0xe1 && memory[0] != 0xe2) || (memory[1] & 0xf0U) != 0x40U ||
        (memory[1] & 0x0cU) != 0)
        return {};
    const std::size_t start = memory[2] == 0 ? 8 : 4;
    const std::size_t size = memory[2] == 0 ? ((static_cast<std::size_t>(memory[6]) << 8U) | memory[7]) * 8
                                            : static_cast<std::size_t>(memory[2]) * 8;
    if (size == 0 || size > memory.size() - start)
        return {};
    return std::pair{start, start + size};
}
Bytes tlv_bytes(const Bytes &content) {
    Bytes result{3};
    if (content.size() < 255)
        result.push_back(static_cast<std::uint8_t>(content.size()));
    else {
        result.push_back(0xff);
        result.push_back(static_cast<std::uint8_t>(content.size() >> 8U));
        result.push_back(static_cast<std::uint8_t>(content.size() & 0xffU));
    }
    result.insert(result.end(), content.begin(), content.end());
    return result;
}
} // namespace
std::optional<Type5Image> parse_type5(std::span<const std::uint8_t> memory) {
    const auto area = bounds(memory);
    if (!area)
        return {};
    Type5Image result{area->first, area->first, area->first, area->second, (memory[1] & 3U) == 0, {}};
    for (std::size_t offset = area->first; offset < area->second;) {
        const auto tlv = next_tlv(memory.first(area->second), offset);
        if (!tlv || tlv->type == 0xfe)
            return {};
        if (tlv->type == 3)
            return extract_ndef(memory, *tlv, result);
        offset = tlv->end;
    }
    return {};
}
std::optional<Bytes> replace_type5(std::span<const std::uint8_t> memory, const Bytes &content) {
    const auto area = parse_type5(memory);
    if (!area || !area->writable || !application::nfc::decode_ndef(content))
        return {};
    const auto replacement = tlv_bytes(content);
    const auto tail = memory.subspan(area->end, area->used - area->end);
    if (replacement.size() + tail.size() > area->limit - area->offset)
        return {};
    Bytes result(memory.begin(), memory.end());
    auto destination = result.begin() + static_cast<std::ptrdiff_t>(area->offset);
    destination = std::copy(replacement.begin(), replacement.end(), destination);
    destination = std::copy(tail.begin(), tail.end(), destination);
    return result;
}
} // namespace manager::adapters::nfc
