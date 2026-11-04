#pragma once
#include "application/nfc/ndef.hpp"

namespace manager::application::nfc {
// Owns the complete message and its optimistic-write expectation. No UI or I/O.
class Draft {
  public:
    Status begin(const Snapshot &state);
    void end();
    bool open() const {
        return open_;
    }
    bool dirty() const {
        return open_ && records_ != original_;
    }
    bool conflict(const Snapshot &state) const {
        return open_ && expected_ != state.content;
    }
    const std::vector<Record> &records() const {
        return records_;
    }
    std::optional<Bytes> bytes() const {
        return encode_ndef(records_);
    }
    Status replace(std::vector<Record> records);
    Status put(int index, Record record);
    bool remove(int index);
    bool move(int from, int to);
    Status duplicate(int index);
    Status prepare(const Snapshot &state, WriteRequest &request) const;

  private:
    Bytes expected_;
    std::vector<Record> records_;
    std::vector<Record> original_;
    bool open_{};
};
Result write_tag(TagPort &tag, const WriteRequest &request);
} // namespace manager::application::nfc
