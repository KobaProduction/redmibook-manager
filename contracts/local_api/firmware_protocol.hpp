#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
namespace manager::contracts::firmware {
enum class Command : unsigned {
    ChargeCapability,
    ReadCharge,
    Charge80,
    Charge100,
    PerformanceCapability,
    ReadPerformance,
    Balanced,
    Quiet,
    Turbo,
    ReadMicrophone,
    MuteMicrophone,
    UnmuteMicrophone
};
enum class Status : unsigned {
    Available,
    NotImplemented,
    UnsupportedHardware,
    UnverifiedFirmware,
    PermissionDenied,
    Unavailable,
    Failed,
    InvalidRequest,
    AppliedUnconfirmed,
    Confirmed,
    OutcomeUnknown,
    Incompatible
};
struct Request {
    Command command;
    std::string version;
    std::string build;
};
struct Reply {
    Command command{};
    Status status{Status::Unavailable};
    int value{-1};
    unsigned allowed{};
    std::uint32_t process{};
    std::string version;
    std::string build;
    std::string detail;
};
bool mutation(Command command);
std::string encode(const Request &request);
std::string encode(const Reply &reply);
std::optional<Request> decode_request(std::string_view message);
std::optional<Reply> decode_reply(std::string_view message);
} // namespace manager::contracts::firmware
