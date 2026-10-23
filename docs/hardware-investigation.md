# Windows hardware investigation

Evidence checkpoint: 2026-10-19. This records observed facts and implementation limits;
class discovery is not successful hardware control.

## Local target

Read-only Windows CIM inventory returned:

| Field | Observation |
| --- | --- |
| Manufacturer/model | XIAOMI / Redmi Book Pro 16 2024 |
| Board/revision | TM2309 / V24C1 |
| SMBIOS BIOS version | RMAMT6B0P0A0A |
| BIOS release date | 2024-06-04 |

No serial number or UUID was collected. No firmware/EC writes, driver installation,
vendor process termination or power-setting changes were performed for this inventory.
`ROOT\WMI` exposes the `MICommonInterface.MiInterface` class/method. Its input is
`InData` (byte array, MAX 32); outputs are `OutData` (byte array, MAX 30) and
**`ReturnCode` (uint16)**. Enumeration of actual instances returned Access denied
under the current unelevated identity.
Do not silently replace permission denial with unsupported hardware.

An administrator-run **read-only** `redmibook-device status` probe subsequently
succeeded on this exact P0A0A BIOS: firmware-reported charge policy was 100% and
performance profile was 1 (Balanced). Both GET replies passed status, selector and
value validation. This is execution evidence for those GETs only: no SET was sent,
no charging cutoff was observed at that stage.
The ignored local evidence is `.tools/native-device-elevated.json`, with elevated
identity and exit 0 recorded in `.tools/native-device-elevated-meta.json`.

Owner-authorized elevated SET qualification subsequently passed on this exact
RMAMT6B0P0A0A BIOS: charge 80/full and profiles 1 (Balanced), 2 (Quiet), 3 (Turbo)
were each accepted and confirmed by immediate GET. Original full-charge/ Balanced
settings were restored and read back. Evidence: `.tools/firmware-write-journal-20261019.json`
and `.tools/firmware-write-trial-result.json`; journal phase `restored`, no unsettled
write. These discrete settings are write-qualified. Profile 4 remains blocked.
This establishes policy readback, not physical cutoff at 80%.

A separate passive physical-key capture on the same board/BIOS received exactly
three pairs for three owner presses of Mi: 32-byte `EventDetail` values beginning
`01 18 01` then `01 19 01`, with zero remaining bytes. Pair gaps were 55–90 ms.
Evidence: `.tools/mi-button-capture.json`. These are leading/trailing events, not
proven hold/release semantics. Decode one action per complete pair; codes 0x25/0x26
from another board must not be substituted.

This corrects a concrete error in the old `origin/research` Rust contract: it names
its trailing field `Reserved`. The installed schema has `ReturnCode`. Discover the
instance dynamically and validate its active MIFS identity; never hard-code MIFS_0.

## Battery range is model-specific

A [firsthand TM2309 investigation](https://lists.openwall.net/linux-kernel/2026/10/08/1628)
uses BIOS RMAMT6B0P0B0B, newer than this installation. It identifies group 0x1000,
subcommand 2 as fixed 80% protection: value 1 enables; every other value disables.
It reports charging observations and warns against generic keyboard command 0x10
mapping. This corroborates the old research branch's fixed 80/full-charge contract,
but is not local acceptance for P0A0A. Never send granular level codes to TM2309.

[XiControl's protocol investigation](https://github.com/Oksion/XiControl/blob/main/docs/01-wmi-protocol.md)
reports granular 40/50/60/70/80/100 thresholds on **TM2424**, not our board. It also
identifies firmware event delivery through ROOT\WMI/HID_EVENT20, EventDetail bytes,
with Mi press/release codes 0x25/0x26 on its tested model. These are candidates for
passive observation on TM2309, not guaranteed key bindings. The public headline
40–100 is neither a universal range nor a continuous slider. XiControl is GPL3;
this MIT repository must author independent implementations from protocol facts,
not copy its source or assets.

## Existing project evidence

`origin/research` at 68ee914fbb59bc928399bacc86a3125e440ed1e6 contains the previous Rust
factory protocol, capability inventory and transport. It was inspected without
merging. Its own docs explicitly say target execution pending. Useful packet facts:

- 32-byte little-endian input; operation GET 0xFA00 / SET 0xFB00 at offset 0;
- selector at offset 2,16-bit subcommand/value0 at offset 4,32-bit value1 at offset 6;
- performance 0x0800, charge 0x1000/sub 2, microphone 0x0A00/sub 5;
- status 0x8000 indicates command acceptance,0xE000 unsupported;
- TM2309 SET may omit selector echo; GET must retain its expected selector;
- fixed battery state is returned at offset 6; profile at offset 4.

The readback establishes the firmware's reported policy, not physical charging
cutoff. Physical acceptance needs an observed battery charging transition at the
limit. Neither a successful WMI HRESULT nor matching policy readback proves that
charging actually stopped.

## Charge retention investigation, 2026-10-20

The canonical Koba Analysis project `RedmiBook_TM2309_Firmware_Analysis`
(`ghp_2bf17493ef188f564a68d8ae`) contains the installed A0A XMCC1806 SSDT.
Its SHA-256 is `18c4a7da3e4874a6cf359bf0863b412daececf90c321b23b7ee389b9c9b3ef4f`.
The 5,773-byte AML was read from that program and disassembled with ACPICA iasl
20230628; no byte patches or replacement imports were made. The scoped conclusion
is saved/read back in that program's Program Information option
`Redmibook Manager charge evidence 2026-10-20`.

WMAA directly handles the battery selector in ASL: GET reads bit 0 of EC field
`LONL`; SET value 1 sets that bit, other values clear it while preserving the
remaining bits. The matching DSDT declares LONL at EC byte 0xA4. In the examined
XMCC1806 and DSDT ASL there is no LONL heartbeat or reset timer; the DSDT contains
only its field declaration. This does not establish the EC firmware's internal
retention rules or rule out writes by other components.

DSDT ECRD/ECWT acquire ECMT with a 1,000 ms firmware-side timeout and require ECAV.
ECRD may return zero on failure and ECWT provides no positive write-completion
result. Thus SET status 0x8000 alone cannot confirm a change; matching GET confirms
the reported bit, not physical cutoff. The separate native SMM provider in
OemODMSmmServiceDriver is a documented stub; that does not contradict the SSDT's
direct EC path. Do not treat every similarly named firmware provider as the active
Windows path.

The linked firsthand TM2309/P0B0B report describes retention across charger replugs,
s2idle and approximately 17 hours, then loss after the battery ran empty. This is
corroboration for a different BIOS, not proof of periodic SET requirements on A0A.
Do not add blind periodic firmware writes. A future restoration policy must retain
explicit owner intent, read before reconciling known drift and respect the core's
unknown-outcome write latch.

Local execution evidence is stored in ignored diagnostics:

- `battery-observation-20261020.json`: installed 1.1.2/native-controls.4, refreshed
  policy 80, AC connected, Charging=false, ChargeRate=0, capacity approximately 97%.
- `battery-toggle-observation-20261020.json`: one authorized 80 → 100 → 80 cycle;
  both writes confirmed by GET, original 80 restored, no unsettled write. A bounded
  WMI change subscription did not observe charging resume under 100 at the existing
  approximately 97% level; restoration to 80 remained non-charging.

Consequently, the command/readback path is executed, but the test does not establish
physical actuation or an 80% threshold crossing. High-charge restart hysteresis,
other EC conditions and control failure remain distinguishable hypotheses; no
unknown command, forced discharge or invented heartbeat was used.

Microphone polarity and reachable fan-RPM provider remain unresolved. Do not expose
raw microphone signal 0/1 as user-facing muted/unmuted. Do not implement arbitrary
ACPI evaluation or EC memory access to fill these gaps.

The subsequent owner-operated event capture establishes narrower facts on P0A0A:
Fn+K emitted `01 16 03` (profile announcement 3), Fn+S emitted `01 1A 00`
(refresh request, no frequency in the payload), and two microphone presses emitted
`01 21 00` / `01 21 01`. The owner observed LED changes while recording continued:
firmware intent is not Windows capture state. Microphone event 0 requests mute;
event 1 requests unmute, subject to explicit Audio API completion/readback. This does
not establish the separate MIFS microphone GET/SET polarity. The profile was restored
to Balanced after the key test (`.tools/after-key-test-profile-restored.json`).

An unelevated 100 ms passive subscription also succeeded: event listening does not
require the elevated identity needed for MIFS instance calls on this installation.
Keep resident session event capture separate from privileged firmware commands.

### Fn+K event routing correction

The saved, disassembled XMCC1806 table has an EV20 case 0x16 that calls FUNR(0x16),
writes the reported profile to QFAN via ECWT, and invokes NTDP before returning the
WMI event payload. WMAA's ordinary performance SET path also invokes QV20(1,0x16).
Thus event 0x16 cannot uniquely identify a physical Fn+K press. Manager exposes
Fn+K as fixed, and profile announcements only update observed state and OSD.
No firmware suppression mechanism has been qualified; do not promise arbitrary
Fn+K reassignment or dispatch saved legacy bindings on these notifications.

## Windows features outside firmware

### Battery telemetry and retained policy

The Windows read-only telemetry adapter enumerates battery-class interfaces and queries
the tagged BatteryInformation and BatteryStatus IOCTLs on a worker. It reports a single
system battery; multiple packs remain unavailable rather than silently choosing one.
Capacity-relative devices do not publish invented Wh/W. Driver unknown sentinels remain
missing; percentage uses remaining/full capacity and wear is an estimate from full/design.
Signed power is positive while charging and negative while discharging. These are battery
driver readings, not adapter wattage or negotiated USB-C power.
The native adapter caches the open battery/tag and metadata for up to 60 seconds;
active sampling reads BatteryStatus only. A failed status invalidates that cached handle.
The session's optional 100/1000/5000 ms sampler has one in-flight worker and publishes only
changed values. The driver can return cached measurements; the selector cannot force
fresh EC data or guarantee real-time completion.

The machine authority persists only explicit qualified charge selections. Charger-source
and resume notifications trigger serialized read-before-reconcile. In 1.1.4, the
five-minute fallback is replaced with an independent machine guard on a ten-second
native waitable timer, using the shared read-only TelemetryPort implementation.
The machine takes its own authoritative battery sample rather than trusting the
session/UI cache; the existing session cache remains a separate reader.
Healthy matching readback never causes a SET. An unknown read never causes a SET.
A durable pending marker is flushed before a
write and cleared only after confirmed GET; an unsettled marker blocks subsequent writes.
The requested policy survives reinstall/restart in the protected ProgramData service folder.
No user policy is inferred from a firmware default at startup.
Each selection and scheduled check appends a UTC JSON event to charge-events.jsonl in
the protected service directory, with reason, observed/desired/after limits and statuses.
At 512 KiB it rotates to charge-events.previous.jsonl, retaining one prior file.
The authenticated charge diagnostic includes the last successfully flushed event and
the guard's last telemetry sample/status with its age; logging
failure is diagnostic and does not invent hardware success. The fallback is retained
as a bounded background guard; successful policy readback alone does not establish
that the owner's reported charging problem is gone.

The resident session publishes telemetry independently of optional UI lifetime, on power
and percentage notifications, with a coarse fallback of ten seconds. Requests coalesce while a worker is active. Firmware polling does not run at
the telemetry cadence; policy reconciliation belongs to the machine authority.

Installed checkpoint 2026-10-21: 1.1.2/native-controls.5 service/session compatibility
passed. Before an explicit new selection, GET already reported 80%; therefore the
owner's reported overcharge does not establish a cleared policy bit. Explicit 80%
selection through the service then confirmed matching GET and persisted intent.
The installed state feed reported 100% (84.599/84.724 Wh), AC online, Charging=false,
0 W and 17.339 V, matching an independent BatteryStatus reading. This verifies the
installed telemetry path and retained selection, not a physical 80% cutoff or a fix
for an EC that continues charging while its policy bit still reads enabled.

### Live over-limit incident, 2026-10-21, installed 1.1.3

An owner-reported incident was reproduced with the running machine service and a
fresh session firmware refresh: GET reported 80%, while Windows independently
reported Charging=true, approximately 20 W and 91% (77.348/84.724 Wh). The service's
last audit recorded desired=observed=after=80 on a power-or-resume reconciliation.
Thus matching policy readback did not establish physical enforcement.

One explicit same-value 80% SET through the authenticated service completed and
matched GET, but charging continued for the following 30-second event observation
(approximately 18 W; remaining energy increased). No repeated-write heartbeat was added.
A subsequent bounded 100% -> 80% command sequence confirmed both readbacks and
restored the persisted desired policy to 80. Windows then reported Charging=false
and ChargeRate=0, approximately one second after the restored 80% command.

This is evidence that an off/on transition recovered enforcement in this incident;
it does not prove the EC's internal failure mechanism, retention rules or an actual
charging crossing at 80%. MaintainChargePolicy::reconcile currently repairs only
readback drift, so a retained 80% bit with ineffective enforcement escapes recovery.
Do not turn this observation into blind periodic toggles or unqualified EC writes.
Effective-state detection and bounded, journaled recovery are implemented for 1.1.4:
with persisted 80% intent, an available AC-online, charging sample at or above 80% and
positive finite W triggers a qualified read. A retained 80% flag uses one confirmed
100 -> 80 transaction; known drift to 100 restores 80 directly. The desired policy
stays 80 and the pending marker spans both phases. Missing confirmation stops writes.
Normal telemetry does not trigger firmware reads/writes. If both phases confirm but
charging still continues, the next ten-second observation may try again; no total
attempt cap is currently imposed. The automatic recurrence path still needs live
acceptance; the observed manual recovery does not prove all future recoveries.

Ignored evidence: .tools/charge-incident-before.json,
.tools/charge-incident-core-status.json, .tools/charge-incident-reapply80.json,
.tools/charge-incident-reapply-observation.json,
.tools/charge-incident-edge100.json, .tools/charge-incident-edge80.json and
.tools/charge-incident-edge-observation.json.

Refresh rate and Windows microphone mute do not require BIOS adapters.
Use [QueryDisplayConfig](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-querydisplayconfig)
to identify the intended display; enumerate valid modes and use
[ChangeDisplaySettingsExW](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-changedisplaysettingsexw)
with an explicit device name, then re-enumerate its current mode. A null device name
would affect the primary display and reproduce the earlier multi-monitor bug.
Do not invent 120 Hz if the actual panel advertises another refresh rate.

[IAudioEndpointVolume::SetMute](https://learn.microsoft.com/en-us/windows/win32/api/endpointvolume/nf-endpointvolume-iaudioendpointvolume-setmute)
controls an endpoint's mute state; pair it with GetMute and default capture-endpoint
selection. Endpoint mute and firmware microphone state/LED are separate until
synchronization has been proven. Generic user hotkeys use RegisterHotKey with explicit
conflict results. Vendor key observation belongs to the resident session, independent
of tray lifetime; do not toggle firmware state twice when firmware already acts.

Use EndpointVolume mute, never disable a device/driver for this feature. Microsoft
documents that EndpointVolume uses a device's hardware mute when provided and
otherwise supplies software mute; software control does not cover exclusive streams.
QueryHardwareSupport reports this distinction, not physical privacy certification.
Read-only local inventory found one ACTIVE capture endpoint, shared by Console,
Multimedia and Communications roles; GetMute was false and hardware support mask 7
includes MUTE. Evidence: `.tools/microphone-inventory.log`. Driver-advertised hardware
mute still needs actual recording acceptance. Multiple microphones require an explicit
target policy/device selection, not a hidden all-device change. Do not label an LED
event as actual mute, infer current firmware state from persisted preferences, or
unmute automatically at startup when authoritative firmware state is unavailable.

Sources: [EndpointVolume behavior](https://learn.microsoft.com/en-us/windows/win32/coreaudio/endpointvolume-api),
[hardware capability query](https://learn.microsoft.com/en-us/windows/win32/api/endpointvolume/nf-endpointvolume-iaudioendpointvolume-queryhardwaresupport),
[independent OEM dispatcher investigation](https://github.com/Oksion/XiControl/blob/main/docs/14-xiaomi-hotkey-osd.md).

## Original product

[Xiaomi's official interoperability page](https://hyperos.mi.com/continuity) is the
vendor distribution/support entry point; it lists Redmi Book Pro 16 2024. Existing
factory PC Manager functions are product context, not a published stable hardware
SDK. Third-party replacements and firmware recovery remain evidence about particular
models. No official proprietary binary or source is being imported into this project.

## Safe implementation sequence

1. Read real identity and publish per-source availability/errors in Manager.
2. Provide narrow CLI probes and a build-tree developer launch path; no installer is
   needed to test application ports or protocol fakes.
3. Qualify exact board+BIOS with administrator-run, read-only GETs. Keep unverified
   firmware distinct from permission denied and unsupported.
4. Enable only verified discrete policy values and explicit user commands. Never
   infer charge/performance intent during discovery. Production startup may reconcile
   a previously explicit, persisted charge selection; missing intent means no write.
5. Compare policy readback; report actual charge cutoff as separately unverified.
6. Passively observe Mi events, then map explicit user bindings. Linux adapters
   initially return NotImplemented; compile-path coverage is not hardware acceptance.

Hardware writes belong to a compatible, authenticated machine authority. A developer
console calling a local adapter directly is a diagnostic tool, not permission for
GUI clients to bypass the installed machine service's authorization boundary.

## Waiting for native WMI completion

The adapter uses Microsoft's [semisynchronous ExecMethod contract](https://learn.microsoft.com/en-us/windows/win32/api/wbemcli/nf-wbemcli-iwbemservices-execmethod):
`WBEM_FLAG_RETURN_IMMEDIATELY` produces an `IWbemCallResult`.
A worker waits once through [GetResultObject](https://learn.microsoft.com/en-us/windows/win32/api/wbemcli/nf-wbemcli-iwbemcallresult-getresultobject)
with a bounded timeout. No Sleep or polling loop is used. `WBEM_S_TIMEDOUT` is a
pending/unknown outcome, never success or proof that an eventual operation was
cancelled. Missing output is also a failure. This bounds the completion wait,
not every preceding COM connection/class-discovery call; callers must keep this
port off the GUI thread and report their own overall deadline honestly.

## Fn+F4 firmware-state read (static reverse, 2026-10-23)

The analyzed TM2309 XMCC1806 SSDT implements a read-only MIFS request
`FA00 / 0A00 / sub 05`. The 32-byte little-endian packet begins
`00 FA 00 0A 05 00 00 00`, with the remaining bytes zero. A successful reply
has status WORD `8000`, selector WORD `0A00`, subcommand WORD `0005`, then
FRD1 DWORD at offset 6. The method reads `Q_EC.FUNR(0x20)` / `MIUT` and returns
its inverse. FRD1 is the same value as byte 2 of event `{01,21,value}`:
`0` requests muted, `1` requests unmuted on this qualified firmware.

Evidence: Koba Analysis project `RedmiBook_TM2309_Firmware_Analysis`
(`ghp_2bf17493ef188f564a68d8ae`), installed 2024-06-04 ACPI artifacts:

- `SSDT_XMCC1806.aml`, SHA256
  `18c4a7da3e4874a6cf359bf0863b412daececf90c321b23b7ee389b9c9b3ef4f`:
  decompiled lines 364–386 (GET), 773–790 (event); AML offsets A10–A46 / 1588–15D0.
- `DSDT_INTEL_SKL.aml`, SHA256
  `68d4ee232bf503f7397f391db5abcfa5ccf91285e9f7582f850306fc1807ba13`:
  FUNR(20) returns MIUT; this is a memory-mapped EC field. Do not add raw physical
  memory access when the qualified MIFS GET already exists.

This establishes firmware-state semantics statically, not a physical LED sensor or
actual Windows endpoint mute. No live GET/SET was performed in this investigation.
The allowlist now exposes a narrow, read-only microphone flag query through
authenticated machine IPC, qualified separately for TM2309/P0A0A. P0B0B is not
qualified for this query merely because charge/performance are qualified.
Subscribe before the initial asynchronous read and discard its stale result if a newer
event arrived. Following live qualification and the owner's LED-authority decision,
known firmware intent reconciles both mute and unmute; unknown state never changes audio. The firmware SET also emits
0x21, so treating every event as a physical key press and echoing it back to firmware
would create a feedback loop. No additional polling is needed.

The session starts the initial query after the HID subscription reports Ready.
Microphone events are counted at native callback ingress, so a queued newer event,
overflow or subscription failure supersedes a late initial result. Known qualified
initial intent schedules endpoint synchronization; unknown/denied reads never change
the endpoint. Subsequent announcements invalidate cached intent and trigger GET,
not an endpoint toggle or a firmware SET. All command callers require actual endpoint readback to match.
The initial-query completion reports whether mute was requested, not whether the
endpoint command completed. Endpoint state/OSD remain based on the audio result.

The diagnostic CLI `firmware-microphone` performs only this qualified GET;
`microphone read` reads only the Windows endpoint. They do not change either state.
A normal user process can receive WMI access denied; production uses the machine
service. The qualified production SET and GET chain is described below. The GET
reads the EC flag associated with the LED, not an independent physical LED sensor.

### Microphone LED write qualification

The follow-up reverse inspection traced DSDT `ECWT` at AML offset 0x34DD9:
it acquires ECMT (1000 ms), stores the argument into the referenced MIUT field
only when ECAV is true, then releases the mutex. It returns no positive write
completion. The outer MIFS SET still reports 0x8000. Therefore that response
does not prove a completed field write or a changed physical LED.

The visible SET path is FB00/0A00/sub5: DWORD 0 requests muted (MIUT=1),
DWORD 1 requests unmuted (MIUT=0), followed by firmware-owned 150 ms sleep
and event 0x21. No codec, microphone power or privacy GPIO operation was
identified in this path. A possible EC reaction to MIUT is unproven.
The owner's earlier LED-on/recording-active observation establishes that the
tested recording path was not muted by the physical key alone. Actual endpoint
mute currently comes from the resident Windows event handler.

Owner policy: Manager and tray expose microphone observation only until a
qualified command can synchronize the physical LED. Before live qualification, the Manager mute switch
and its invokable endpoint-only command were removed. The qualified service-backed
switch is restored by the production integration below. Physical Fn+F4
announcements continue to drive Windows endpoint mute. Diagnostic CLI operations
remain explicit diagnostic tools, not a qualified LED control feature.
No firmware microphone SET is admitted to production based on static analysis.

#### Live qualification, 2026-10-23

The owner authorized an isolated elevated diagnostic using
`tools/diagnose-microphone-led.ps1`. Identity was checked as XIAOMI / TM2309 /
RMAMT6B0P0A0A before either write. The installed resident session remained active.

- Mute: firmware GET false -> SET mute -> GET true. Owner confirmed physical LED
  lit and recording stopped. Windows endpoint readback separately returned muted=true.
- Unmute: firmware GET true -> SET unmute -> GET false. Owner confirmed LED off
  and recording restored. Windows endpoint readback separately returned muted=false.

Evidence: `.tools/microphone-led-toggle-1.json`,
`.tools/microphone-led-toggle-endpoint.json`, `.tools/microphone-led-unmute-2.json`,
`.tools/microphone-led-unmute-endpoint.json`, plus the owner's two confirmations.
The microphone was restored to its original unmuted state.

This supersedes the earlier unqualified SET-to-LED relationship for this exact
hardware/firmware: software SET controls the physical indicator in both directions.
It does not qualify other BIOS versions or prove that firmware itself disconnects
the audio path, because the resident event-to-Windows-mute handler was active.
MIUT is now a qualified firmware intent/indicator state, not a separate optical LED
sensor. Production control must SET through the authenticated machine service,
confirm GET, and apply the observed desired state through the session audio adapter.
Events and readback must not echo another SET; stale reads must not overwrite newer
events. Preserve distinct firmware/indicator intent and actual endpoint state on
failure or external Windows mute changes. The diagnostic helper alone does not
restore a production Manager switch or extend the production command allowlist.

#### Qualified production integration

The TM2309/P0A0A integration now admits explicit MuteMicrophone/UnmuteMicrophone
commands through the authenticated, build-checked machine service. Other BIOS
versions remain unqualified. Every request first verifies a readable qualified
flag, executes one SET, then confirms GET. Missing readback or an unknown native
write outcome is not success and blocks subsequent firmware writes; a known
opposite readback is a mismatch, not permission to retry automatically.

Manager's switch projects the observed firmware flag and only sends service
commands. Actual Windows mute remains a separate observed value. The resident
session treats 0x21 as an invalidation, reads the current firmware flag on its
command worker, and changes the audio endpoint only when it differs. It never
echoes SET from an event. The optional custom microphone hotkey uses the same
firmware service commands. The diagnostic CLI's explicit endpoint commands remain
separate tools for troubleshooting.

Known firmware intent now reconciles both mute and unmute at startup, superseding
the earlier conservative mute-only startup rule following the owner's LED-authority
decision. Unknown, denied or superseded reads never authorize an endpoint change.
The existing ten-second session reconciliation schedules a bounded synchronization
to recover missed events without another timer, sleeps or repeated SETs. Background
checks do not generate OSD or control busy state. Native endpoint notifications
continue to publish the actual audio state to Manager and tray.
