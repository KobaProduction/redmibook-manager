# Diagnostic local protocol v1

These text protocols are implemented by the diagnostic scaffold. They are not a
hardware command API. Application ports are standard C++; wire parsing stays in adapters.
Only one request/response is allowed per connection. No localized text or arbitrary
program arguments travel over either endpoint.

## Machine endpoint (Windows)

Named pipe: KobaProduction.RedmibookManager.Diagnostic.v1. The server runs as LocalService,
rejects remote clients and grants access to local interactive users, administrators,
SYSTEM and LocalService. It accepts only `RMB/1 PING` with its terminal NUL byte.
Response: `RMB/1 PONG <version> <build-id> <pid>`.

The client uses overlapped bounded I/O and identification-only security quality of
service. It verifies the actual named-pipe server PID against SCM's registered
RedmibookManager process, then validates the response protocol/verb/PID. A successful
connection or arbitrary PONG string is not sufficient evidence of the daemon.
The listener is single-instance and processes bounded requests serially. This is
diagnostic health, not hardware acceptance or a firmware-write authorization policy.

## Session endpoint

Qt local server name: KobaProduction.Redmibook.Session.<identity digest>. The digest
scopes the endpoint by user/session; it is not authentication. QLocalServer UserAccessOption
sets the OS access boundary. Requests are newline-terminated and bounded to 128 bytes:

- `RMB/1 status`: perform a fresh machine health probe, without OSD.
- `RMB/1 diagnostic`: perform a fresh probe, then show labelled diagnostic OSD.
- `RMB/1 open-tray`: request a separate optional tray client.
- `RMB/1 restart`: acknowledge then restart the session runtime under its current user.
- `RMB/1 shutdown`: acknowledge then stop only the session runtime.

Response: `RMB/1 OK <session-pid> <up|down> <daemon-pid> <version|-> <build-id|->` plus newline.
Control acknowledgements include the last observed machine status, not a new health
probe. Status/diagnostic produce a fresh result. Client responses are bounded to 512
bytes with a 3-second deadline. The session server bounds idle connections to 4 seconds.
Unknown commands are disconnected. There are no performance, charge or shell commands.

Manager and tray singleton/activation endpoints are session/user-scoped. Tray close
requests affect only the tray client; they do not forward shutdown to the session host.
Language/theme synchronization uses atomic, locked per-user preferences and directory
change observation; it does not make the machine daemon own user appearance.

These baseline user-access endpoints do not yet provide the finer per-session caller
authorization/event feed needed for future hardware commands and multiuser policy.
Extend those contracts and tests before enabling such operations.
