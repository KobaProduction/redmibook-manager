#pragma once
#include "adapters/windows/firmware/mifs_transport.hpp"
#include "application/microphone/firmware_state_port.hpp"
namespace manager::adapters::windows_firmware {
application::microphone::FirmwareState read_microphone_state(MifsTransport &transport,
                                                             const FirmwareIdentity &identity);
application::microphone::FirmwareState request_microphone_state(MifsTransport &transport,
                                                                const FirmwareIdentity &identity, bool muted);
} // namespace manager::adapters::windows_firmware
