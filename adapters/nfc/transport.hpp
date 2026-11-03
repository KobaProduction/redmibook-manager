#pragma once
#include "application/nfc/port.hpp"
#include <span>
#include <stop_token>

namespace manager::adapters::nfc {
using application::nfc::Bytes;
using application::nfc::Status;
struct Discovery {
    Status status{Status::Absent};
    bool present{};
    std::string name{"NXP NTAG5"};
};
class Transport {
  public:
    virtual ~Transport() = default;
    virtual Discovery discover() = 0;
    virtual Status read(std::uint16_t block, std::span<std::uint8_t> result) = 0;
    virtual Status write(std::uint16_t block, std::span<const std::uint8_t> bytes) = 0;
    virtual Status read_register(std::uint16_t block, std::uint8_t index, std::uint8_t &value) = 0;
    virtual bool has_events() const = 0;
    virtual Status wait_event(std::stop_token stop) = 0;
    virtual Status configure_events(bool) {
        return Status::Ready;
    }
    virtual bool events_active() const {
        return false;
    }
};
} // namespace manager::adapters::nfc
