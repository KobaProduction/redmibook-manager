#include "adapters/nfc/type5.hpp"
#include "application/nfc/ndef.hpp"
#include <iostream>

namespace {
namespace app = manager::application::nfc;
bool mixed_message() {
    const app::Record vendor{4, {'x', ':', 'v'}, {0, 255}, {0, 128, 255, 1}};
    const auto text = app::decode_ndef(app::make_text_record("Hello"));
    const auto uri = app::decode_ndef(app::make_uri_record("https://example.org/"));
    if (!text || !uri)
        return false;
    auto records = *text;
    records.insert(records.begin(), vendor);
    records.push_back(uri->front());
    const auto bytes = app::encode_ndef(records);
    const auto decoded = bytes ? app::decode_ndef(*bytes) : std::nullopt;
    return decoded && *decoded == records && decoded->front() == vendor;
}
bool invalid_records() {
    app::Record record{6, {'X'}, {}, {}};
    if (app::encode_ndef(std::vector{record}))
        return false;
    record = {1, app::Bytes(256, 'T'), {}, {}};
    if (app::encode_ndef(std::vector{record}))
        return false;
    return true;
}
bool identifier_edges() {
    app::Record record{5, {}, app::Bytes(255, 1), {3, 4}};
    const auto bytes = app::encode_ndef(std::vector{record});
    const auto decoded = bytes ? app::decode_ndef(*bytes) : std::nullopt;
    record.id.push_back(1);
    const bool rejected = !app::encode_ndef(std::vector{record});
    record.id.pop_back();
    const auto empty = app::encode_ndef({});
    return decoded && decoded->front() == record && empty && empty->empty() && rejected;
}
bool boundaries() {
    app::Record record{1, {'X'}, {}, app::Bytes(2041, 'x')};
    const auto bytes = app::encode_ndef(std::vector{record});
    if (!bytes || bytes->size() != 2048 || !app::decode_ndef(*bytes))
        return false;
    record.payload.push_back('x');
    return !app::encode_ndef(std::vector{record}) && invalid_records() && identifier_edges();
}
app::Bytes type5_image(bool terminator) {
    app::Bytes image(1032, 0);
    image[0] = 0xe1;
    image[1] = 0x40;
    image[2] = 0x80;
    image[4] = 3;
    if (terminator)
        image[6] = 0xfe;
    return image;
}
bool type5_limit(bool terminator) {
    const auto image = type5_image(terminator);
    const auto parsed = manager::adapters::nfc::parse_type5(image);
    if (!parsed)
        return false;
    const auto limit = manager::adapters::nfc::max_type5_content(*parsed);
    app::Record record{1, {'T'}, {}, app::Bytes(limit - 7, 'x')};
    const auto exact = app::encode_ndef(std::vector{record});
    if (!exact || !manager::adapters::nfc::replace_type5(image, *exact))
        return false;
    record.payload.push_back('x');
    const auto oversized = app::encode_ndef(std::vector{record});
    return oversized && !manager::adapters::nfc::replace_type5(image, *oversized);
}
} // namespace
int main() {
    const bool passed = mixed_message() && boundaries() && type5_limit(false) && type5_limit(true);
    if (!passed)
        std::cerr << "NDEF message / storage boundary contract failed\n";
    return passed ? 0 : 1;
}
