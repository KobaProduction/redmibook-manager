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
tray/Manager clients. The Windows scaffold implements diagnostic routing and OSD;
real key capture and hardware control remain unimplemented. Closing tray/Manager
does not stop session diagnostics/OSD or the machine host. Logical roles do not fix
executable counts or make Windows APIs part of shared application contracts.

Read [component interfaces](component-contracts.md) before implementing adapters and
[process lifecycle](process-lifecycle.md) for Windows/Linux host mappings and upgrades.
The machine runtime has no Qt rendering and cannot execute arbitrary user-configured
programs. Session execution uses the user's own identity.

## Platform contract

The UI is shared across Windows and Linux. OS integrations are separate adapters,
not duplicated UI trees. Current adapters identify the compile-time host only;
they do not probe battery or Mi-key support. Unsupported hardware must be visible,
and an accepted firmware request is not proof of an effective charge limit.

## Workflow provenance

Agent workflow links are pinned to revisions selected by Qt Visualization.
clang-tidy complexity thresholds and the adapted Chromium checkdeps implementation
come from that reference. Audio, DSP, deployment and user-account rules were not copied.
Third-party checkdeps provenance and license live beside its source.
