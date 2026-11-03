#pragma once
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace manager::application::nfc {
using Bytes = std::vector<std::uint8_t>;
enum class Status : std::uint8_t {
    Ready,
    Absent,
    Unavailable,
    Denied,
    Failed,
    TimedOut,
    Busy,
    Conflict,
    ReadOnly,
    InvalidData,
    UnknownOutcome,
    Incompatible
};
struct Snapshot {
    Status status{Status::Unavailable};
    bool present{};
    std::string name;
    std::string uid;
    std::optional<bool> field;
    std::uint64_t encounters{};
    std::uint32_t capacity{};
    bool writable{};
    bool event_driven{};
    Bytes content;
    bool operator==(const Snapshot &) const = default;
};
struct WriteRequest {
    Bytes expected;
    Bytes content;
};
struct Result {
    Status status{Status::Unavailable};
    Snapshot snapshot;
};
class Subscription {
  public:
    virtual ~Subscription() = default;
};
// Native operations are serialized by the session on a worker. Subscriptions only
// invalidate state: callbacks never read hardware and may arrive on a native thread.
// Destroying a subscription stops and joins its bounded, cancellable native wait.
class TagPort {
  public:
    virtual ~TagPort() = default;
    virtual Snapshot read() = 0;
    virtual Result write(const WriteRequest &request) = 0;
    virtual std::unique_ptr<Subscription> subscribe(std::function<void()> changed) = 0;
};
// Client callbacks run on the client's owning event loop. One observer, no replayed
// encounter notifications. Destroying the client cancels callbacks, not native I/O.
class ClientPort {
  public:
    virtual ~ClientPort() = default;
    virtual void observe(std::function<void(const Snapshot &)> observer) = 0;
    virtual void refresh() = 0;
    virtual bool write(WriteRequest request, std::function<void(Status)> completion) = 0;
};
} // namespace manager::application::nfc
