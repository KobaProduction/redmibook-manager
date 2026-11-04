#include "application/nfc/draft.hpp"
#include <algorithm>

namespace manager::application::nfc {
Status Draft::begin(const Snapshot &state) {
    if (open_)
        return Status::Busy;
    if (state.status != Status::Ready)
        return state.status;
    const auto records = decode_ndef(state.content);
    if (!records)
        return Status::InvalidData;
    expected_ = state.content;
    records_ = original_ = *records;
    open_ = true;
    return Status::Ready;
}
void Draft::end() {
    open_ = false;
    expected_.clear();
    records_.clear();
    original_.clear();
}
Status Draft::replace(std::vector<Record> records) {
    if (!open_ || !encode_ndef(records))
        return Status::InvalidData;
    records_ = std::move(records);
    return Status::Ready;
}
Status Draft::put(int index, Record record) {
    if (!open_ || index < -1 || index >= static_cast<int>(records_.size()))
        return Status::InvalidData;
    auto next = records_;
    if (index == -1)
        next.push_back(std::move(record));
    else {
        record.id = next[static_cast<std::size_t>(index)].id;
        next[static_cast<std::size_t>(index)] = std::move(record);
    }
    return replace(std::move(next));
}
bool Draft::remove(int index) {
    if (!open_ || index < 0 || index >= static_cast<int>(records_.size()))
        return false;
    records_.erase(records_.begin() + index);
    return true;
}
bool Draft::move(int from, int to) {
    const auto size = static_cast<int>(records_.size());
    if (!open_ || from < 0 || to < 0 || from >= size || to >= size)
        return false;
    if (from < to)
        std::rotate(records_.begin() + from, records_.begin() + from + 1, records_.begin() + to + 1);
    else
        std::rotate(records_.begin() + to, records_.begin() + from, records_.begin() + from + 1);
    return true;
}
Status Draft::duplicate(int index) {
    if (!open_ || index < 0 || index >= static_cast<int>(records_.size()))
        return Status::InvalidData;
    auto next = records_;
    next.insert(next.begin() + index + 1, records_[static_cast<std::size_t>(index)]);
    return replace(std::move(next));
}
Status Draft::prepare(const Snapshot &state, WriteRequest &request) const {
    if (state.status != Status::Ready)
        return state.status;
    if (!state.writable)
        return Status::ReadOnly;
    if (!open_ || !dirty())
        return Status::InvalidData;
    if (conflict(state))
        return Status::Conflict;
    const auto encoded = bytes();
    if (!encoded || encoded->size() > state.max_content_size)
        return Status::InvalidData;
    request = {expected_, *encoded};
    return Status::Ready;
}
Result write_tag(TagPort &tag, const WriteRequest &request) {
    const auto state = tag.read();
    if (state.status != Status::Ready)
        return {state.status, state};
    if (state.content != request.expected)
        return {Status::Conflict, state};
    if (!decode_ndef(request.content) || request.content.size() > state.max_content_size)
        return {Status::InvalidData, state};
    if (state.field != false)
        return {Status::Busy, state};
    if (!state.writable)
        return {Status::ReadOnly, state};
    return tag.write(request);
}
} // namespace manager::application::nfc
