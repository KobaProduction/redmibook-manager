# Windows development runtime

`tools/dev-session.ps1` runs the build-tree machine diagnostic host, resident
session, optional tray and optional Manager as the current user. It does not
install or update a package, change SCM, request elevation or register autostart.
The foreground development host exposes diagnostic PING only; it is not a
privileged firmware execution path. Startup never applies hardware settings.
On qualified TM2309/P0A0A, the session now listens for physical vendor keys as the
current user. Incoming F4/Fn+S events can change microphone mute/internal-panel
refresh; closing Manager/tray does not stop this subscription. Mi emits OSD in the
isolated development scope; automatic child launching remains blocked there to
preserve launcher process ownership. Installed/non-isolated Mi opens Manager.

Use PowerShell 7 from the checkout. First startup configures a separate Debug
build under `build/dev` with payload identity `dev-local`, then builds the four
runtime targets and their dependencies. Subsequent builds are incremental.
Pass the installed Qt 6.10.2 MinGW directory through `QT_ROOT` or `-QtRoot`.

```powershell
cd <checkout>
$env:QT_ROOT = 'C:/Qt/6.10.2/mingw_64'
./tools/dev-session.ps1 -Action start -Manager
```

Stop before rebuilding an executable currently in use. A focused iteration can
build one selected target rather than the package, tests or quality aggregate.
The selected target still builds changed transitive dependencies through CMake.
Use `-Target core`, `session`, `tray` or `manager` for the build action; the
default `all` builds the runtime executables only. `-SkipBuild` uses existing
binaries, whose reported session/core version and build must agree.

```powershell
cd <checkout>
./tools/dev-session.ps1 -Action stop
./tools/dev-session.ps1 -Action build -Target manager
./tools/dev-session.ps1 -Action start -SkipBuild -Manager
```

`-Action restart` stops this launch's tracked processes and incrementally builds
and starts them again. `-NoTray` leaves the session/OSD running without tray.
`-Manager` opens the main UI; the default starts the session and tray only.
There is no default microphone binding. Pass `-MicrophoneHotkey 'Ctrl+Alt+M'`
to opt into that session's microphone shortcut. The shortcut can change the real
microphone state when pressed; startup itself does not toggle it.
`-Action status` makes one bounded status request and prints the result without
displaying a diagnostic OSD.
`-BuildDirectory` selects an existing alternative build tree consistently for
build/start. Stop/status recover the original build path from the process ledger.

## Isolation and ownership

The controller keeps a per-checkout scope token, process ledger and logs in
ignored `.tools/dev-runtime`. The scope remains stable across restarts so test
preferences persist. GUI preferences and update history live under the platform's
generic user-data directory in `Koba Production/Redmibook Manager Development/<scope>`.
All session, tray and Manager endpoints/locks derive from the scoped identity.
The Windows diagnostic pipe has the same scope suffix; the launcher supplies the
exact development core PID to its children. Production diagnostics continue to
authenticate the core through SCM. Invalid development scope values fail instead
of falling back to production endpoints.

The launcher awaits asynchronous named-pipe connection readiness with cancellation
deadlines, then checks core/session identity once before starting the clients.
It obtains the session endpoint through `redmibook-session --endpoint`; it does not
duplicate the native user/session identity or hashing algorithm. Process completion
uses asynchronous exit notifications with cancellation deadlines. There are no
fixed startup sleeps or repeated status polls. No shell commands or firmware
parameters cross that diagnostic pipe. The installed service and installed clients
can remain running independently.

Stop examines only ledger entries with matching PID, process creation time and
executable path, requests GUI closure and terminates an unresponsive owned process
after a bounded wait. It never kills by executable basename or stops the installed
service. Clients manually replaced/restarted outside this launcher are not claimed
as owned; close those development clients explicitly before removing the ledger.
No wildcard process cleanup or service recovery is performed.

In a development scope, UI actions do not spawn detached components. Open Manager
through the launcher with `-Manager`; use its `restart` action to restart the stack.
Applying an interface style saves the preference and closes Manager; restart it
through the launcher to keep process ownership explicit. The development UI cannot
restart the installed machine runtime or invoke installation/service maintenance.

This loop replaces packaging/installation during ordinary development, not final
quality gates or hardware acceptance. Run only the checks affected by a concrete
change while iterating; follow `development.md` for the final verification cadence.
Native Linux runtime acceptance remains deferred.
