#include "contracts/local_api/firmware_protocol.hpp"
#include <string>
namespace {
namespace wire = manager::contracts::firmware;
bool request_contract() {
    const wire::Request request{wire::Command::Charge80, "1.1.1", "fixture-A"};
    const auto decoded = wire::decode_request(wire::encode(request));
    return decoded && decoded->command == request.command && decoded->version == request.version &&
           decoded->build == request.build && wire::mutation(request.command) &&
           !wire::mutation(wire::Command::ReadCharge) && !wire::mutation(wire::Command::ReadPerformance);
}
bool microphone_request_contract() {
    const wire::Request request{wire::Command::ReadMicrophone, "1.1.5", "fixture-A"};
    const auto decoded = wire::decode_request(wire::encode(request));
    const wire::Request write{wire::Command::UnmuteMicrophone, "1.1.5", "fixture-A"};
    const auto written = wire::decode_request(wire::encode(write));
    return written && written->command == write.command && decoded && decoded->command == request.command &&
           !wire::mutation(decoded->command) && wire::mutation(wire::Command::MuteMicrophone) &&
           wire::mutation(wire::Command::UnmuteMicrophone);
}
bool reject_unsafe_requests() {
    using namespace std::string_view_literals;
    return !wire::decode_request("RMB/2 FW 12 1.1.1 fixture-A") &&
           !wire::decode_request("RMB/2 FW -1 1.1.1 fixture-A") &&
           !wire::decode_request("RMB/2 FW 2 1.1.1 fixture-A extra") &&
           !wire::decode_request("RMB/2 FW 2 1.1.1 fixture-A\0extra"sv) &&
           !wire::decode_request(std::string(513, 'x'));
}
bool reply_contract() {
    wire::Reply reply;
    reply.command = wire::Command::ReadCharge;
    reply.status = wire::Status::PermissionDenied;
    reply.process = 321;
    reply.version = "1.1.1";
    reply.build = "fixture-A";
    reply.detail = "provider denied access \"without success\"";
    const auto decoded = wire::decode_reply(wire::encode(reply));
    return decoded && decoded->status == reply.status && decoded->detail == reply.detail &&
           decoded->value == -1 && decoded->process == reply.process;
}
} // namespace
int main() {
    return request_contract() && microphone_request_contract() && reject_unsafe_requests() && reply_contract()
               ? 0
               : 1;
}
