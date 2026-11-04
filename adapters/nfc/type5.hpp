#pragma once
#include "application/nfc/port.hpp"
#include <span>

namespace manager::adapters::nfc {
struct Type5Image {
    std::size_t offset{};
    std::size_t end{};
    std::size_t used{};
    std::size_t limit{};
    bool writable{};
    application::nfc::Bytes content;
};
[[nodiscard]] std::uint32_t max_type5_content(const Type5Image &image);
[[nodiscard]] std::optional<Type5Image> parse_type5(std::span<const std::uint8_t> memory);
[[nodiscard]] std::optional<application::nfc::Bytes> replace_type5(std::span<const std::uint8_t> memory,
                                                                   const application::nfc::Bytes &content);
} // namespace manager::adapters::nfc
