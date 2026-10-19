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

## Windows features outside firmware

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
   change charge/performance automatically during discovery or startup.
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
