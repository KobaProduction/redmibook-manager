#include "contracts/local_api/firmware_protocol.hpp"
#include <iomanip>
#include <sstream>
namespace manager::contracts::firmware {
namespace {
bool finished(std::istream &stream) {
    stream >> std::ws;
    return stream.eof();
}
bool identity(const std::string &value) {
    return !value.empty() && value.size() <= 96 &&
           value.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789._-") ==
               std::string::npos;
}
} // namespace
bool mutation(Command command) {
    return command == Command::Charge80 || command == Command::Charge100 || command == Command::Balanced ||
           command == Command::Quiet || command == Command::Turbo || command == Command::MuteMicrophone ||
           command == Command::UnmuteMicrophone;
}
std::string encode(const Request &request) {
    return "RMB/2 FW " + std::to_string(static_cast<unsigned>(request.command)) + " " + request.version +
           " " + request.build;
}
std::string encode(const Reply &reply) {
    std::ostringstream output;
    output << "RMB/2 FW " << static_cast<unsigned>(reply.command) << ' ' << reply.version << ' '
           << reply.build << ' ' << static_cast<unsigned>(reply.status) << ' ' << reply.value << ' '
           << reply.allowed << ' ' << reply.process << ' ' << std::quoted(reply.detail);
    return output.str();
}
std::optional<Request> decode_request(std::string_view message) {
    if (message.empty() || message.size() > 512 || message.find('\0') != std::string_view::npos) {
        return {};
    }
    std::istringstream input{std::string(message)};
    std::string protocol, kind;
    unsigned command{};
    Request result{};
    if (!(input >> protocol >> kind >> command >> result.version >> result.build) || !finished(input) ||
        protocol != "RMB/2" || kind != "FW" || command > static_cast<unsigned>(Command::UnmuteMicrophone) ||
        !identity(result.version) || !identity(result.build)) {
        return {};
    }
    result.command = static_cast<Command>(command);
    return result;
}
std::optional<Reply> decode_reply(std::string_view message) {
    if (message.empty() || message.size() > 512 || message.find('\0') != std::string_view::npos) {
        return {};
    }
    std::istringstream input{std::string(message)};
    std::string protocol, kind;
    unsigned command{}, status{};
    Reply result;
    if (!(input >> protocol >> kind >> command >> result.version >> result.build >> status >> result.value >>
          result.allowed >> result.process >> std::quoted(result.detail)) ||
        !finished(input) || protocol != "RMB/2" || kind != "FW" ||
        command > static_cast<unsigned>(Command::UnmuteMicrophone) ||
        status > static_cast<unsigned>(Status::Incompatible) || !identity(result.version) ||
        !identity(result.build) || result.allowed > 7 || result.process == 0) {
        return {};
    }
    result.command = static_cast<Command>(command);
    result.status = static_cast<Status>(status);
    return result;
}
} // namespace manager::contracts::firmware
