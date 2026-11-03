#include "adapters/platform/nfc.hpp"
#include "application/nfc/ndef.hpp"
#include <atomic>
#include <chrono>
#include <fstream>
#include <future>
#include <iostream>
#include <string_view>

namespace {
void report(const manager::application::nfc::Snapshot &state) {
    std::cout << "status=" << static_cast<int>(state.status) << " present=" << state.present
              << " writable=" << state.writable << " capacity=" << state.capacity
              << " contentBytes=" << state.content.size() << " field=";
    if (state.field)
        std::cout << *state.field;
    else
        std::cout << "unknown";
    std::cout << '\n';
}
bool backup(const manager::application::nfc::Bytes &bytes, const char *path) {
    std::ofstream stream(path, std::ios::binary | std::ios::out | std::ios::trunc);
    stream.write(reinterpret_cast<const char *>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    stream.flush();
    return stream.good();
}
int events(const std::shared_ptr<manager::application::nfc::TagPort> &tag) {
    auto ready = std::make_shared<std::promise<void>>();
    auto first = std::make_shared<std::atomic<bool>>(true);
    auto completion = ready->get_future();
    auto watch = tag->subscribe([ready, first] {
        if (first->exchange(false))
            ready->set_value();
    });
    if (!watch || completion.wait_for(std::chrono::seconds(3)) != std::future_status::ready)
        return 5;
    const auto armed = tag->read();
    watch.reset();
    const auto stopped = tag->read();
    std::cout << "eventSubscription=" << armed.event_driven << " stopped=" << !stopped.event_driven << '\n';
    return armed.event_driven && !stopped.event_driven ? 0 : 6;
}
int roundtrip(const std::shared_ptr<manager::application::nfc::TagPort> &tag,
              const manager::application::nfc::Snapshot &original, const char *path) {
    namespace nfc = manager::application::nfc;
    if (original.status != nfc::Status::Ready || !nfc::decode_ndef(original.content) ||
        !backup(original.content, path))
        return 2;
    const auto trial = nfc::make_text_record("Redmibook Manager NFC qualification");
    const auto written = tag->write({original.content, trial});
    std::cout << "trialStatus=" << static_cast<int>(written.status) << '\n';
    if (written.status != nfc::Status::Ready)
        return 3;
    const auto restored = tag->write({trial, original.content});
    std::cout << "restoreStatus=" << static_cast<int>(restored.status) << '\n';
    return restored.status == nfc::Status::Ready && tag->read().content == original.content ? 0 : 4;
}
} // namespace
int main(int argc, char **argv) {
    namespace nfc = manager::application::nfc;
    const auto tag = manager::adapters::make_nfc_tag();
    const auto original = tag->read();
    report(original);
    if (argc == 2 && std::string_view(argv[1]) == "--events")
        return events(tag);
    if (argc == 3 && std::string_view(argv[1]) == "--roundtrip")
        return roundtrip(tag, original, argv[2]);
    return original.status == nfc::Status::Ready || original.status == nfc::Status::Absent ? 0 : 1;
}
