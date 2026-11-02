#include "adapters/local_api/session_codec.hpp"
#include "application/runtime/build_identity.hpp"

int main() {
    using namespace manager;
    const application::ComponentIdentity identity{manager::application::build_identity::version,
                                                  manager::application::build_identity::payload};
    const application::DiagnosticReply core{true, manager::application::build_identity::version,
                                            manager::application::build_identity::payload, 25};
    const auto wire = adapters::encode_session_reply(core, 42, identity);
    const auto reply = adapters::decode_session_reply(wire);
    if (reply.process_id != 42 || reply.daemon.process_id != 25 ||
        application::connection_health(reply, identity) != application::ConnectionHealth::Ready) {
        return 1;
    }
    const auto legacy = adapters::decode_session_reply(QByteArray("RMB/1 OK 42 up 25 ") +
                                                       manager::application::build_identity::version + " " +
                                                       manager::application::build_identity::payload + "\n");
    if (!legacy.connected ||
        application::connection_health(legacy, identity) != application::ConnectionHealth::VersionMismatch) {
        return 1;
    }
    return adapters::decode_session_reply("RMB/1 OK invalid up 25 v b v b").connected ||
           adapters::decode_session_reply("RMB/1 OK 42 invalid 25 v b v b").connected ||
           adapters::decode_session_reply("RMB/1 OK 42 up").connected;
}
