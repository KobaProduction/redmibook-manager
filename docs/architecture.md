# Architecture and source navigation

## Dependency rule

`apps → presentation / adapters → application → domain`.

Domain and application have no Qt, Windows or Linux API dependency. CMake targets
express link dependencies; `DEPS` files enforce literal include boundaries.
Platform selection happens in CMake, not in QML or domain conditionals.

## Target module layout

[Module layout and dependency contracts](module-layout.md) owns the planned directory
tree, domain boundaries, data ownership, cross-module interaction, build targets and
composition scopes. It is a blueprint: the table below describes today's bootstrap.
Do not create empty module scaffolding or move files before the relevant implementation.

[Qt UI conventions](qt-ui-conventions.md) governs English/Russian localization,
System/Light/Dark appearance, presentation-controller placement and native Qt APIs.

| Concern | Owner |
| --- | --- |
| Host-platform vocabulary | `domain/system/` |
| Read overview and host-information port | `application/overview/` |
| Windows/Linux implementations | `adapters/windows/`, `adapters/linux/` |
| Shared adapter declaration | `adapters/platform/` |
| Manager view model and QML pages | `presentation/manager_model.*`, `presentation/qml/` |
| Runtime diagnostic contracts | `application/runtime/` |
| Preference contracts, persistence and shared Qt controllers | `application/ui_preferences/`, `adapters/storage/ui_preferences/`, `presentation/common/` |
| Session IPC and diagnostic routing | `adapters/local_api/` |
| Windows machine host/maintenance | `adapters/windows/hosting/` |
| Independent OSD and tray/dashboard UI | `presentation/osd/`, `presentation/tray/` |
| Release identity and update policy | `application/updates/` |
| Public GitHub feed, daily history and architecture selection | `adapters/updates/` |
| About update state / independent update notice | `presentation/updates/` |
| Observed SMBIOS identity | `application/device_profile/`, `adapters/device_profile/`, `adapters/{windows,linux}/device_profile/` |
| Charge / vendor performance policies and ports | `domain/{battery,performance}/`, `application/{battery,performance}/` |
| Qualified Xiaomi MIFS commands | `adapters/windows/firmware/`, `adapters/windows/{battery,performance}/` |
| Session microphone / display controls | `application/{microphone,display}/`, `adapters/platform/session_controls.hpp`, Windows/Linux adapters |
| Opt-in session hotkeys | `application/shortcuts/`, `adapters/{windows,linux}/shortcuts/`, `apps/session/input_actions.*` |
| Process composition | `apps/desktop/`, `apps/session/`, `apps/tray/`, `apps/machine/` |
| Quality and packages | `cmake/`, `tools/`, `.github/workflows/` |

The scaffold has Manager, tray, session and machine executables. The Windows machine
host implements diagnostic IPC only; Linux machine integration reports unavailable.
DDD vocabulary is used
where there is domain meaning; there are no placeholder aggregates, repositories,
event buses or databases. Battery and hotkey contexts will own their actual invariants
when implemented. A separate folder alone is not a bounded context.
The `core` CMake preset builds the inner layers and native platform contract tests
without finding or linking Qt. This is an executable check of framework independence.

## Process contract

Windows installation paths, data ownership and privilege boundaries are specified in
[the Windows installation contract](windows-installation.md).

The design separates machine runtime, resident session runtime/OSD and optional
tray/Manager clients. The Windows machine host implements diagnostic routing and
authenticated qualified firmware commands;
qualified vendor input, audio/display controls and OSD run in the user session.
Closing tray/Manager
does not stop session diagnostics/OSD or the machine host. Logical roles do not fix
executable counts or make Windows APIs part of shared application contracts.

Read [component interfaces](component-contracts.md) before implementing adapters and
[process lifecycle](process-lifecycle.md) for Windows/Linux host mappings and upgrades.
The machine runtime has no Qt rendering and cannot execute arbitrary user-configured
programs. Session execution uses the user's own identity.

## Platform contract

The UI is shared across Windows and Linux. OS integrations are separate adapters,
not duplicated UI trees. Windows observes SMBIOS identity, microphone state and
display modes; Linux native control adapters explicitly return NotImplemented.
Vendor charge/performance reads and writes have separate compatibility gates;
firmware identity is not inferred from a translated display label. See
[hardware investigation](hardware-investigation.md). Qualified vendor events are decoded
behind the application input port and routed in resident session composition; Linux
returns NotImplemented. An accepted firmware request is not proof of an effective charge limit.

The development CLI is an explicit composition for native-adapter diagnostics, not a
production privileged command endpoint. [Development runtime](dev-runtime.md) starts
isolated diagnostic hosts without installing/registering anything. Hardware changes
never occur merely because the diagnostic core or session starts.

## Workflow provenance

Agent workflow links are pinned to revisions selected by Qt Visualization.
clang-tidy complexity thresholds and the adapted Chromium checkdeps implementation
come from that reference. Audio, DSP, deployment and user-account rules were not copied.
Third-party checkdeps provenance and license live beside its source.
