# Product and interface contract

## Scope and supported hardware

RedmiBook Manager is a native C++20/Qt Quick laptop manager. Current base version:
**1.1.5**. Windows acceptance targets Redmi Book Pro 16 2024, XIAOMI TM2309/V24C1,
BIOS RMAMT6B0P0A0A. Linux retains shared contracts and build/package paths; deep
Linux desktop/hardware acceptance is deferred. No WebView, cloud account or usage
telemetry is required. See [validation](validation.md) for the acceptance boundary
and [hardware investigation](hardware-investigation.md) for qualification evidence.

## Interface

Navigation order is Overview, Device, Hotkeys, Settings, About. Overview projects
assembled controls and observed device state. Device combines battery telemetry,
charge limit/manual energy saver, microphone, internal display and useful known
BIOS/system-board identity. No duplicate Diagnostics or separate Battery navigation
page is needed. About owns runtime/version details, connectivity and licenses.
Unknown identity fields and absent capabilities are omitted, not filled with
unsupported-control placeholder cards.

Manager uses Fluent controls, System/Light/Dark appearance and System/English/Russian
language selection. Changes apply live across clients through daemon preferences.
Light selected controls use a black accent; custom drawing inherits centralized
palettes. The startup shell applies the saved palette before exposure and loads
content asynchronously behind a centered animated spinner, without skeletons or
loading text. Offline/retry states stay in the same window. Follow
[Qt UI conventions](qt-ui-conventions.md).

Left-click tray toggles its compact dashboard; right-click opens its menu. The
header combines title/version and compact monochrome power, charge-policy,
microphone and connection information, subject to visibility preferences. Device
controls show observed state, not an optimistic successful write. Closing either
client leaves session hotkeys, OSD and machine policies running. The vendor key
opens and activates Overview by default, including an already-open Manager.

## Controls and presets

- Charge policies are fixed 80% and full charge. Do not send 40–100% codes from
  other boards to TM2309. User intent persists at the machine authority; an
  accepted transport call or matching policy bit alone does not prove cutoff.
- The independent ten-second machine guard checks telemetry. Charging at or above
  80% with selected protection triggers confirmed recovery: restore drifted policy
  directly, or use a confirmed 100 → 80 sequence when the 80% bit was retained.
  Healthy readings cause no SET. Unknown outcomes block further firmware writes
  until reconciliation, including across restart. No five-minute fallback remains.
- Battery telemetry includes charge state/percentage, capacities, estimated wear,
  voltage and directional rate in W. Energy uses Wh. Battery details request
  100 ms/1 sec/5 sec sampling only while visible; open tray requests one second.
- Firmware profiles are Quiet, Balanced and Performance. Native profile 3 may be
  called Turbo in protocol/research sources. FullSpeed/profile 4 remains blocked.
  Do not promise specific CPU clocks, undervolting or energy savings from a label.
- Only a uniquely identified built-in panel is controlled, using enumerated modes
  and readback. Users select the rates used in cycling; external/ambiguous sources
  are refused. Reset does not change the current display mode.
- Qualified microphone control SETs and confirms the firmware intent/LED flag via
  the machine service. Session synchronization applies the observed intent to the
  Windows communications endpoint without disabling the driver. Physical events
  invalidate/read state, never echo another SET. Unknown or stale reads cannot
  authorize audio changes. Flag and actual endpoint state remain distinct.
- Hotkeys expose configurable application actions, including no action and opening
  Manager/tray. Fn+K is a fixed firmware-owned profile control; its announcement
  must not execute a second command. Fn+S is a session-managed refresh action.
- Windows manual energy saver is distinct from firmware profiles and Windows power
  schemes. It is runtime-qualified and confirmed through native observation. Do
  not change automatic thresholds or administrative policy. Brightness is a preset
  action, not a duplicate standalone Windows brightness slider.

[Tray presets](tray-presets.md) owns the extensible ordered-action contract and
[Windows power controls](windows-power-controls.md) owns native qualification.
The daemon creates one editable, localized Power/Economy/Performance starter preset,
hidden from tray by default. User captions remain literal. Actions are preflighted,
run in order and stop on unconfirmed results without rolling back earlier changes.
Reset restores the hidden factory preset and full charging while preserving the
current display mode. Settings has expanded tray configuration and independent
OSD visibility and sound controls; all feedback shares one presenter.

## Authorities, background work and installation

The resident session daemon owns user preferences, keeps validated revisions in RAM,
broadcasts accepted changes and coalesces atomic persistence. Clients communicate
through IPC, never through settings-file reads/writes/watches or locks. The machine
service separately owns privileged firmware policy and its write journal. Offline,
incompatible, unconfirmed and denied states stay distinct. See
[preference authority](preference-authority.md) and [component contracts](component-contracts.md).

Native commands/queries run on workers with completion events and bounded deadlines;
GUI deadlines do not prove cancellation. Native notifications drive updates and
one bounded ten-second reconciliation recovers missed events. Background reads do
not flicker busy state or disable controls. Required battery sampling/charge guard
are explicit exceptions to event-only observation. See [process lifecycle](process-lifecycle.md).

Windows setup is elevated and machine-wide under
`Program Files/Koba Production/Redmibook Manager`; Manager/tray run unelevated.
Writable state stays in LocalAppData/ProgramData. Updates preserve preferences and
verify session/core product/build compatibility. Distribution uses the visible
installer; explicitly requested local automation may use `tools/install-dev.ps1`.
See [installation contract](windows-installation.md).

## About, licenses and updates

About attributes Koba Production, links source and provides local application and
dependency licenses. Application code is MIT; dependency licenses remain separate.
The Russian MIT text is informational. Manual and optional daily release checks use
the public GitHub API with bounded responses/deadlines. The resident session performs
at most one persisted automatic check per 24 hours. A user click opens the matching
OS/architecture download or release page; the checker does not install automatically.
The alpha/stable comparison and immutable build rules live in [release policy](releasing.md).

## Version ownership

`VERSION.txt` is the single base-version source. Owner-authorized checkpoint commits
may increment patch at most once. Previews, builds and installations retain the
current version and use distinct payload identities. CI never bumps versions.
Major/minor and stable publication require explicit owner decisions. Preserve the
independent remote research branch; it is not part of this implementation.
