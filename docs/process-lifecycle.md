# Process lifecycle and update acceptance

## Platform-independent roles

Define shared interfaces before implementing Windows/Linux hosts. Product roles
do not depend on SCM, systemd, tray availability or a fixed executable count.
The current diagnostic scaffold has separate Manager, tray, resident session/OSD
and machine executables. Windows provides native diagnostic IPC; Linux machine
hosting remains unavailable. Keep version 0.1.0.

| Component | Responsibility | Lifetime |
| --- | --- | --- |
| Machine runtime (daemon) | Authorized hardware policy/commands and device-state events | Independent of UI and login where supported |
| Session runtime | User-session input, action routing, result subscription, feedback dispatch | Resident during graphical session, independent of tray/settings |
| OSD presenter | Brief state feedback without stealing focus | Available while session background functions are enabled |
| Tray client | Optional dashboard/menu, status, opening Manager | Can close without stopping background functions |
| Manager client | Overview, settings section and diagnostics | On demand, independently closable |

These are logical modules, not five required executables. Composition roots choose
OS-specific process boundaries while preserving privilege/lifetime boundaries.
Tray/settings may share a client process. Session runtime may load an OSD renderer
on demand or supervise a helper. Destroying tray must not destroy input/event
subscriptions or the OSD host. Do not keep the settings QML engine resident merely
to maintain background operation.

Closing tray, closing settings, disabling OSD, disabling bindings and stopping
session background functions are distinct operations. Do not label background
shutdown as merely closing tray. Killing all session components necessarily removes
our input/custom OSD until supervision restarts them: rendering requires a live
renderer or desktop notification server. Session exit differs from daemon shutdown.

Manager and tray retain a shared presentation health policy and poll diagnostics
every two seconds; the session client has a three-second request timeout. Keep
session unavailable, core unavailable and incompatible version/build distinct.
Compare both product version and payload build against the core's authenticated
reply; equal base versions alone do not establish matching binaries. An unavailable
or incompatible core remains visible in the dashboard/menu/Manager instead of
silently retaining a healthy state. Diagnostic access remains available.
The session coalesces up to eight concurrent probes into one core request and
returns the result to every waiting live client, coalescing diagnostic feedback.

Tray replacement is serialized per user/session: hold a handover lock, request
graceful shutdown through the owned local endpoint, wait for the instance lock,
listen as the new owner, then release handover. Never display two tray icons or
kill arbitrary processes by executable name. If the old tray cannot stop within
the bounded wait, report failure and leave it in place.

## Platform adapters

| Concern | Windows | Linux |
| --- | --- | --- |
| Machine host | SCM service adapter | System host, normally systemd on supported distributions |
| Session host | User-logon/session adapter | Graphical-session adapter, systemd user or desktop autostart |
| Input | Verified hotkey/device/vendor adapter | Wayland portal/compositor or X11 adapter; device events where justified |
| Feedback | Session-owned OSD and separate OS notification adapter | Desktop/compositor-supported OSD and notification adapters |
| Local transport | Authenticated local named pipe | Authenticated D-Bus or Unix socket, choose before implementing |

Windows services cannot render ordinary desktop UI directly. A privileged Linux
daemon must not acquire the user's display connection as its rendering strategy.
Device events may originate in the machine runtime; ordinary shortcuts and desktop
state may originate in the session runtime. Both use common typed contracts.
Do not route every key through a privileged daemon or assign all feedback to tray.

Linux interfaces/adapter boundaries are required now; Linux environment preparation
and runtime acceptance remain deferred. Wayland permissions/backend, tray and overlay
availability are capabilities, not universal promises. Never silently fall back to
root input capture when a desktop denies a shortcut. OSD and notifications are distinct;
a notification fallback must report that limitation. Desktop security/compositor rules
may prevent OSD over secure/exclusive surfaces; do not promise unrestricted display.

## Interfaces and event flow

Read [component contracts](component-contracts.md) before adapter implementation.
Inner contracts use standard C++; Qt/OS/transport types remain outside.

Input/device event -> normalized intent -> authorized use case -> verified result/
state event -> session feedback policy -> OSD/notification presenter.

An event may report state already changed by firmware/OS, such as Caps/Fn-lock.
Observe its authoritative state instead of toggling it again. A press alone does
not prove a change. Scope events to the affected machine/session/user, correlate
duplicates and discard stale transient events after reconnect. Tray/settings are
optional subscribers, never required routers for background functionality.

Local IPC is bounded/versioned with timeouts, cancellation and reconnect. Authorize
actual caller identity, not a SID claimed by a message. Restrict endpoint ACLs and
remote access. Privileged APIs never accept arbitrary shell commands, executable
paths or unrestricted firmware writes. User-assigned programs run under that user's
identity in the session runtime. Decide machine-policy permissions before hardware use.

## Platform-independent update contract

Same-build reinstall is only a reinstall check. Upgrade acceptance uses different
payloads A/B, changed hashes/build identities and obsolete-file fixtures while product
version can remain 0.1.0. Build, product, IPC and schema versions are distinct.

1. Stage/validate B, serialize updates and preserve data/recovery information.
2. Enter authorized maintenance. Quiesce owned runtimes and clients through host
   adapters; wait for actual stop with timeouts. Prevent supervision/autostart races.
   Never kill unrelated processes by basename.
3. Replace owned payload files and remove obsolete files using a manifest. Preserve
   user/machine settings and apply safe versioned migrations.
4. Restore previously enabled hosts in the right identity/session. Verify readiness,
   build identity and protocol compatibility. Restore tray/settings only if previously
   present, retaining closed/hidden state. Never run user UI elevated.
5. Report success only after health checks. Recover the previous consistent payload/
   state on failure where possible; explicitly report incomplete recovery. Do not
   report healthy installation after partial replacement or locked files.

Windows binds this to SCM/session adapters and evaluates Restart Manager for files
in use. Linux binds it to supported system/user hosts and package lifecycle.
NSIS is a Windows tool, not the shared lifecycle contract or automatic rollback.
A network update checker remains a separate feature.

## Acceptance

- Close tray/settings: supported diagnostic input still causes an action and confirmed
  OSD without client dependency. Label test events; never imply actual hardware changes.
- External state change: observe and display once without executing a second toggle.
- Session runtime restart: restore subscriptions/current state, discard stale popups,
  and isolate events between users/sessions. Coalesce repeated input.
- Running A-to-B upgrade: hashes match B, obsolete fixtures disappear, restarted
  processes report B identity, settings survive, input/feedback work, no duplicates.
- Preserve closed clients and administrator-disabled/stopped hosts under an explicit
  update policy; never silently re-enable them.
- Inject unresponsive process, file lock, restart failure and incompatible IPC/schema;
  verify bounded failure/recovery.
- Uninstall unregisters owned system/session hosts, stops owned components and removes
  payload/shortcuts; preserve settings by default, never remove the shared vendor root.
- Portable mode never implicitly installs service/autostart; show absent capabilities.

Implement portable contracts/use cases with fakes before platform behavior. Then add
Windows lifecycle, session input/OSD and upgrades. Keep Linux compile paths and honest
capability reporting as interfaces grow; defer Linux installation acceptance as agreed.

## Evidence

- [Windows service/UI separation](https://learn.microsoft.com/en-us/windows/win32/services/interactive-services)
- [Linux system and user managers](https://www.freedesktop.org/software/systemd/man/latest/systemd.html)
- [Global Shortcuts portal](https://flatpak.github.io/xdg-desktop-portal/docs/doc-org.freedesktop.portal.GlobalShortcuts.html)
- [Desktop Notifications](https://specifications.freedesktop.org/notification-spec/latest/)
- [Windows Restart Manager](https://learn.microsoft.com/en-us/windows/win32/rstmgr/about-restart-manager)

Xiaomi's [notebook policy](https://pc.mi.com/privacy) names OSD Utility for physical-key
state feedback, including Caps Lock. Its [PC Manager policy](https://privacy.mi.com/PCManager/zh_CN/)
mentions hotkey-service versioning. These confirm named functions, not the process/IPC
topology of the current model's PC Manager. A scoped local service/process inventory
found no Xiaomi/MiOSD matches; vendor components were not installed, stopped or altered.
Exact runtime ownership remains unverified and must not be invented.

Windows NSIS setup and uninstall acquire the same Global maintenance mutex in
.onInit/un.onInit, before registry or payload changes. A second process fails closed
with exit code 2. The owning process keeps its handle until exit, so failure/abort
releases the lock through normal Windows handle cleanup.
