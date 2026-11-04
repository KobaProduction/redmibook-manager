#include "application/nfc/ndef.hpp"

namespace manager::application::nfc {
namespace {
bool valid(const Record &record) {
    if (record.tnf >= 6 || record.type.size() > 255 || record.id.size() > 255 || record.payload.size() > 2048)
        return false;
    if (record.tnf == 0)
        return record.type.empty() && record.id.empty() && record.payload.empty();
    return record.tnf == 5 ? record.type.empty() : !record.type.empty();
}
void append_record(Bytes &bytes, const Record &record, std::uint8_t bounds) {
    const bool short_record = record.payload.size() < 256;
    bytes.push_back(static_cast<std::uint8_t>(bounds | record.tnf | (short_record ? 0x10 : 0) |
                                              (record.id.empty() ? 0 : 8)));
    bytes.push_back(static_cast<std::uint8_t>(record.type.size()));
    if (short_record)
        bytes.push_back(static_cast<std::uint8_t>(record.payload.size()));
    else
        for (const unsigned int shift : {24U, 16U, 8U, 0U})
            bytes.push_back(static_cast<std::uint8_t>((record.payload.size() >> shift) & 0xffU));
    if (!record.id.empty())
        bytes.push_back(static_cast<std::uint8_t>(record.id.size()));
    bytes.insert(bytes.end(), record.type.begin(), record.type.end());
    bytes.insert(bytes.end(), record.id.begin(), record.id.end());
    bytes.insert(bytes.end(), record.payload.begin(), record.payload.end());
}
} // namespace
std::optional<Bytes> encode_ndef(std::span<const Record> records) {
    Bytes bytes;
    for (std::size_t index = 0; index < records.size(); ++index) {
        if (!valid(records[index]))
            return {};
        const auto bounds =
            static_cast<std::uint8_t>((index == 0 ? 0x80 : 0) | (index + 1 == records.size() ? 0x40 : 0));
        append_record(bytes, records[index], bounds);
        if (bytes.size() > 2048)
            return {};
    }
    return bytes;
}
} // namespace manager::application::nfc
