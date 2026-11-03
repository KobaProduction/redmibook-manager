#include "application/nfc/ndef.hpp"
#include <algorithm>
#include <limits>

namespace manager::application::nfc {
namespace {
class Cursor {
  public:
    explicit Cursor(std::span<const std::uint8_t> bytes) : bytes_(bytes) {}
    std::optional<std::uint32_t> number(std::size_t count) {
        if (count > bytes_.size() - position_)
            return {};
        std::uint32_t result{};
        for (std::size_t index = 0; index < count; ++index)
            result = (result << 8U) | bytes_[position_++];
        return result;
    }
    std::optional<Bytes> take(std::size_t count) {
        if (count > bytes_.size() - position_)
            return {};
        const auto data = bytes_.subspan(position_, count);
        position_ += count;
        return Bytes(data.begin(), data.end());
    }
    bool done() const {
        return position_ == bytes_.size();
    }

  private:
    std::span<const std::uint8_t> bytes_;
    std::size_t position_{};
};
std::optional<Record> read_record(Cursor &cursor, std::uint32_t flags) {
    const auto type_size = cursor.number(1);
    const auto payload_size = cursor.number((flags & 0x10U) != 0 ? 1 : 4);
    const auto id_size = (flags & 0x08U) != 0 ? cursor.number(1) : std::optional<std::uint32_t>{0};
    if (!type_size || !payload_size || !id_size)
        return {};
    auto type = cursor.take(*type_size);
    auto id = cursor.take(*id_size);
    auto payload = cursor.take(*payload_size);
    if (!type || !id || !payload)
        return {};
    return Record{static_cast<std::uint8_t>(flags & 7U), std::move(*type), std::move(*id),
                  std::move(*payload)};
}
bool valid_record(const Record &record) {
    if (record.tnf >= 6 || (record.tnf == 5 && !record.type.empty()))
        return false;
    if (record.tnf > 0 && record.tnf < 5 && record.type.empty())
        return false;
    return record.tnf != 0 || (record.type.empty() && record.id.empty() && record.payload.empty());
}
Bytes encode_record(std::uint8_t type, Bytes payload) {
    if (payload.size() > 2000)
        return {};
    const bool short_record = payload.size() < 256;
    Bytes result{static_cast<std::uint8_t>(short_record ? 0xd1 : 0xc1), 1};
    if (short_record)
        result.push_back(static_cast<std::uint8_t>(payload.size()));
    else
        for (const unsigned int shift : {24U, 16U, 8U, 0U})
            result.push_back(static_cast<std::uint8_t>((payload.size() >> shift) & 0xffU));
    result.push_back(type);
    result.insert(result.end(), payload.begin(), payload.end());
    return result;
}
} // namespace
std::optional<std::vector<Record>> decode_ndef(std::span<const std::uint8_t> bytes) {
    if (bytes.size() > 2048)
        return {};
    Cursor cursor(bytes);
    std::vector<Record> records;
    while (!cursor.done()) {
        const auto flags = cursor.number(1);
        if (!flags || ((*flags & 0x80U) != 0) != records.empty() || (*flags & 0x20U) != 0)
            return {};
        auto record = read_record(cursor, *flags);
        if (!record || !valid_record(*record) || ((*flags & 0x40U) != 0) != cursor.done())
            return {};
        records.push_back(std::move(*record));
    }
    return records;
}
Bytes make_text_record(const std::string &text) {
    Bytes payload{2, 'e', 'n'};
    payload.insert(payload.end(), text.begin(), text.end());
    return encode_record('T', std::move(payload));
}
Bytes make_uri_record(const std::string &uri) {
    if (uri.empty())
        return {};
    Bytes payload{0};
    payload.insert(payload.end(), uri.begin(), uri.end());
    return encode_record('U', std::move(payload));
}
} // namespace manager::application::nfc
