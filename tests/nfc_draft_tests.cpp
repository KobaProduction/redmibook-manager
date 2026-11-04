#include "application/nfc/draft.hpp"
#include <iostream>
#include <stdexcept>

namespace {
namespace app = manager::application::nfc;
app::Snapshot snapshot() {
    app::Snapshot state;
    state.status = app::Status::Ready;
    state.present = state.writable = true;
    state.field = false;
    state.capacity = 1024;
    state.max_content_size = 1020;
    const auto content = app::encode_ndef(std::vector{app::Record{4, {'x', ':', 'v'}, {0, 255}, {128, 255}}});
    if (!content)
        throw std::logic_error("Invalid NDEF fixture");
    state.content = *content;
    return state;
}
app::Record text_record(const std::string &value) {
    const auto records = app::decode_ndef(app::make_text_record(value));
    if (!records || records->empty())
        throw std::logic_error("Invalid text fixture");
    return records->front();
}
bool preserves_and_cancels() {
    const auto state = snapshot();
    app::Draft draft;
    app::WriteRequest request;
    if (draft.begin(state) != app::Status::Ready || draft.begin(state) != app::Status::Busy ||
        draft.put(-1, text_record("new")) != app::Status::Ready ||
        draft.prepare(state, request) != app::Status::Ready)
        return false;
    const auto saved = app::decode_ndef(request.content);
    const auto original = app::decode_ndef(state.content);
    const bool preserved =
        saved && original && saved->front() == original->front() && request.expected == state.content;
    draft.end();
    return preserved && !draft.dirty() && draft.records().empty() &&
           draft.put(-1, text_record("closed")) == app::Status::InvalidData;
}
bool guards_conflicts_and_limits() {
    auto state = snapshot();
    app::Draft draft;
    app::WriteRequest request;
    const auto original = app::decode_ndef(state.content);
    if (!original || original->empty())
        return false;
    if (draft.begin(state) != app::Status::Ready ||
        draft.put(0, text_record("replacement")) != app::Status::Ready ||
        draft.records().front().id != original->front().id)
        return false;
    state.field = true;
    if (draft.conflict(state))
        return false;
    state.max_content_size = 1;
    if (draft.prepare(state, request) != app::Status::InvalidData)
        return false;
    state.content = app::make_text_record("external");
    return draft.conflict(state) && draft.prepare(state, request) == app::Status::Conflict;
}
class Tag final : public app::TagPort {
  public:
    app::Snapshot state{snapshot()};
    int writes{};
    app::Snapshot read() override {
        return state;
    }
    app::Result write(const app::WriteRequest &) override {
        ++writes;
        return {app::Status::UnknownOutcome, state};
    }
    std::unique_ptr<app::Subscription> subscribe(std::function<void()>) override {
        return {};
    }
};
bool write_boundary() {
    Tag tag;
    const app::WriteRequest request{tag.state.content, app::make_text_record("new")};
    tag.state.field.reset();
    if (app::write_tag(tag, request).status != app::Status::Busy || tag.writes != 0)
        return false;
    tag.state.field = false;
    tag.state.max_content_size = 1;
    if (app::write_tag(tag, request).status != app::Status::InvalidData || tag.writes != 0)
        return false;
    tag.state.max_content_size = 1020;
    return app::write_tag(tag, request).status == app::Status::UnknownOutcome && tag.writes == 1;
}
} // namespace
int main() {
    try {
        const bool passed = preserves_and_cancels() && guards_conflicts_and_limits() && write_boundary();
        if (!passed)
            std::cerr << "NFC draft/application contract failed\n";
        return passed ? 0 : 1;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
