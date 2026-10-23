# Daemon-owned user preferences

A single per-user preference role, hosted by one resident session daemon, owns user settings. It loads
the existing per-user settings.json once, validates requests in its event thread,
and keeps the authoritative revision in RAM. The machine host remains the authority
for privileged charge/performance policies; it never adopts a user's GUI settings
file as a firmware policy.

Manager and tray use PreferenceClient through the standard-C++ PreferencePort.
They neither read nor write settings.json and do not watch its directory or use
preference-file locks. The local file-store mode is for isolated tests and previews.

The Preferences local socket is scoped to the user data directory/development scope (not the terminal session) and
uses UserAccessOption. Every frame carries protocol, application version and payload
build identity. The server accepts only an explicit key allowlist, bounds frames,
peers and queued output, and rejects incompatible requests. Destructive reset and
whole preset-collection replacement carry an expected revision to reject stale writes.

An accepted mutation updates RAM and broadcasts a complete validated snapshot to
subscribers. Acceptance does not mean durable disk completion. The daemon combines
changes over 250 ms and performs atomic QSaveFile persistence on a worker, with at
most one write outstanding. Failed persistence retains dirty RAM state, reports the
failure to clients and retries after ten seconds; it never silently discards intent.
Orderly shutdown finishes the outstanding write and writes any newer dirty snapshot.
An abrupt crash can lose the explicitly deferred batch.

Clients reconnect and obtain a new snapshot. They reject writes while offline or
incompatible, and report asynchronous rejection/timeouts instead of using local
persistence. Manager loads the first snapshot before selecting the Qt Quick style
and creating its UI. All user settings, including presets, language,
appearance, OSD, tray visibility and hotkey assignments, use this path.

Connection availability is independent of the last cached snapshot. The port exposes
connecting, unavailable, incompatible and ready states and a separate change feed.
No cached snapshot authorizes an offline mutation. Manager prepares one native
window immediately, then exposes it after the initial preference snapshot has applied
the saved palette, or after the bounded connection request reports failure. This avoids
showing a light frame before a saved dark theme. Main content loads asynchronously
behind a centered animated spinner, with no loading text or skeletons. No artificial
minimum spinner duration is added. The initial shell
imports only Qt Quick; Qt Quick Controls content loads after the daemon snapshot
initializes the Fluent style. Legacy Classic preferences do not select another style. There is no separate startup connection dialog and
no client-side settings-file read to choose a style. Connection failure and Retry
remain inside that same window. The same Preferences/controller owners survive loading.
After disconnection, Manager retains its window and diagnostic About page, disables
controls, and shows a retry banner. Tray keeps Open Manager and reconnect accessible
while disabling its device/preset actions. Its offline notice is never hidden by the
user's optional connection-icon setting. Both clients obtain fresh snapshots when
reconnected; they never save offline edits for later replay.

The first session runtime acquires the per-user preference-role lifetime lease and
hosts the authority. Other sessions of that same user connect to its common endpoint;
they never construct independent file stores. This is runtime singleton ownership,
not a lock around settings operations. Manager/tray do not acquire this lease.
Session input/OSD runtimes and their diagnostic endpoints remain session-specific.
Every session consumes the preference channel and starts input/OSD only after the
first compatible snapshot. A connection failure schedules one nonblocking ownership
attempt; client reconnect backoff drives further attempts, with no polling loop.
Graceful handover stops commands and flushes dirty state before releasing its role lease;
a successor loads the last disk snapshot. Abrupt failure retains the documented
bounded deferred-write loss window. A live incompatible authority is never displaced.
The session CLI exposes --preferences-endpoint separately from --endpoint.
