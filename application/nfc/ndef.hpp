#pragma once
#include "application/nfc/port.hpp"
#include <span>

namespace manager::application::nfc {
struct Record {
    std::uint8_t tnf{};
    Bytes type;
    Bytes id;
    Bytes payload;
    bool operator==(const Record &) const = default;
};
[[nodiscard]] std::optional<Bytes> encode_ndef(std::span<const Record> records);
[[nodiscard]] std::optional<std::vector<Record>> decode_ndef(std::span<const std::uint8_t> bytes);
[[nodiscard]] Bytes make_text_record(const std::string &text);
[[nodiscard]] Bytes make_uri_record(const std::string &uri);
} // namespace manager::application::nfc
