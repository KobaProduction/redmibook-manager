# Capability inventory

Target: Xiaomi Redmi Book Pro 16 2024 / TM2309.

This inventory tracks user-facing capabilities rather than raw firmware identifiers. The firmware repository remains the authority for the underlying evidence.

## Readiness states

- **Contract ready** — static target evidence closes the semantic contract for its declared availability. This may be a live runtime operation or a persisted firmware policy; the availability field decides which.
- **Execution pending** — static contract is ready but has not yet been exercised on this exact target through Manager.
- **Partial** — the route exists, but scaling, polarity, provider, or safe write behavior is unresolved.
- **Future firmware** — intended for OpenFirmwareBackend; not available as a confirmed factory-firmware operation.

## Factory-firmware capability matrix

| Capability | Read | Write | State | Notes |
| --- | --- | --- | --- | --- |
| Performance / cooling profile | yes | yes | Contract ready; same-model execution corroborated; local execution pending | Balanced, quiet, performance/Turbo and full-speed profiles are mapped to one canonical platform state. TM2309 runtime testing independently confirms mode application. |
| Battery charge protection | yes | yes | Contract ready; same-model execution corroborated; local execution pending | TM2309 runtime testing independently confirms 80% protection and transition back to 100% when cleared. |
| Adapter-power threshold status | yes | no | Partial | Static threshold behavior is known; physical unit/meaning of the raw source is not fully closed. |
| Microphone-mute platform state | yes | yes | Partial | Static route is closed; outward boolean polarity still needs target execution proof. The Rust factory backend includes a neutral State0/State1 validation transport, but the capability remains unpublished to UI until local polarity proof. |
| Keyboard-backlight state | yes/event | no confirmed safe write | Partial | State/event values are known; safe software setter is still unresolved. |
| CPU fan #1 speed | yes | no | Contract ready; Windows transport pending | Intel PTID operating-state element 1 is explicitly labelled CPU Fan #1 Speed, unit RPM. The semantic contract is closed; the preferred Windows user-mode transport still must be selected. |
| CPU fan duty | yes | no | Partial | Value source is known, but scaling is RAW rather than proven percent. |
| Skin temperature 0 | yes | no | Partial | Value source is known, but RAW-to-Celsius conversion is unresolved. |
| Second physical fan speed | unknown | unknown | Partial | Factory Setup displays a GPU-fan field, but no independently proven runtime provider is mapped yet. |
| USB charging mode | persisted policy | persisted policy | Contract ready; live OS apply pending | Setup +0xF3 is 0 Off / 1 Always on / 2 One time only. HQDxeService applies it to EC AOUF during boot; no live OS setter is proven. |
| USB charging threshold | persisted policy | persisted policy | Contract ready; live OS apply pending | Setup +0xF4 is 10/20/30%. HQDxeService applies it to EC UCBT during boot; no live OS setter is proven. |
| Internal-keyboard wake | persisted policy | persisted policy | Contract ready; live OS apply pending | Setup +0xE9 maps to EC IKBW during boot-time synchronization. |
| Wake-on-USB | persisted policy | persisted policy | Contract ready; live OS apply pending | Setup +0xEA maps to EC WOUB during boot-time synchronization. |
| Keyboard-backlight policy | persisted policy | persisted policy | Contract ready; live OS apply pending | Setup +0x102: Standard / Power Saving. HQDxeService maps it to EC KBMD; this is separate from live KBLL backlight state. |
| Display configuration | native route confirmed; OS transport pending | native route confirmed; OS transport pending | Contract ready statically; transport pending | Native firmware handler maps selector 0x0B00 to the confirmed Setup field, but same-model WMAA execution exposes only 0x08, 0x0A/5 and 0x10. The native handler is therefore not published as a Windows runtime capability until a separate reachable transport is proven. |
| Auto fan preset | persisted policy | persisted policy | Partial | Setup +0xF5/+0xF6: Gaming / Normal / Office. Immediate BIOS route persists and refreshes UI/model; hardware apply route is not yet proven. |
| Turbo fan speed preset | persisted policy | persisted policy | Partial | Setup +0xF7/+0xF8: Max / Medium with raw value 2 as a platform compatibility alias for Medium. Hardware apply route is not yet proven. |
| S5 wake configuration | route exists | route exists | Partial | OEM command exists, but exact value semantics are not yet closed. |

## Future OpenFirmwareBackend capabilities

These are design goals, not current factory-firmware claims:

- configurable fan curves;
- richer per-fan control;
- richer temperature/sensor export;
- processor undervolting where hardware permits it;
- power-limit tuning;
- memory tuning;
- optional controller policy for USB/Thunderbolt/USB4;
- explicit Intel Management Engine policy;
- additional low-level telemetry intentionally exported by project firmware.

## Backend rule

FactoryFirmwareBackend and OpenFirmwareBackend must expose the same semantic capability when they implement the same user concept.

For example:

- PerformanceProfile.Quiet remains the same Manager value whether the factory backend maps it to the recovered OEM route or project firmware implements it through a new interface.
- CpuFan1SpeedRpm remains an RPM telemetry capability even if the transport changes.

Raw WMI selectors, ACPI names, embedded-controller offsets, GUIDs and protocol slots belong only to backend implementation/evidence layers.

## Immediate implementation candidates

The first FactoryFirmwareBackend slice should target, in order:

1. capability discovery and firmware identity;
2. performance/cooling profile read/write;
3. battery charge-protection read/write;
4. CPU fan #1 RPM read;
5. keyboard-backlight state/events;
6. microphone-mute state/control after polarity execution proof.

Partial capabilities should not be exposed as writable UI controls until their missing contract is closed.


## Factory-backend compatibility rules

TM2309 must use a model-specific command table.

Do not assume that generic MIFS/Bitland functions are implemented merely because a generic driver defines them. Same-model execution testing shows that the operating-system-visible firmware implements only a narrow subset and returns unsupported status for several generic fan, temperature, keyboard and GPU operations.

The backend must:

- probe or statically declare support per semantic capability;
- treat unsupported status as unavailable rather than zero-valued telemetry;
- use TM2309-specific interpretation for function 0x10, which is the battery/power group on this board;
- not expose generic keyboard-mode controls that collide numerically with the battery charge-protection command;
- tolerate the TM2309 SET-response quirk where the operation is applied and status reports success but the returned function identifier is not echoed as on GET.

These rules are backend behavior; the UI should only see supported/unsupported semantic capabilities.


## Telemetry transport split

FactoryFirmwareBackend is not one transport.

For TM2309 the confirmed factory interfaces are intentionally split:

- factory WMI is used for supported platform controls such as performance/cooling profile and battery charge protection;
- native firmware contains additional internal selector handlers such as 0x0900, but same-model execution shows 0x09 is unsupported through the operating-system-visible WMAA route; internal handlers are therefore not Manager capabilities by themselves;
- generic MIFS fan RPM, manual fan control and CPU-temperature functions are unavailable on this board;
- CPU fan #1 RPM is exposed by Intel PTID/ACPI, not the MIFS command surface;
- CPU temperature for the desktop UI may use the operating system / Intel thermal stack when that is more reliable than a private firmware route.

The Manager capability model must hide this transport split from the UI.

For Windows, CpuFan1SpeedRpm remains implementation-blocked only on choosing a safe access transport to the PTID method. If the installed Intel PTID provider does not expose a suitable user-mode interface, a minimal read-only privileged helper is allowed. Such a helper must expose named telemetry operations rather than arbitrary ACPI evaluation or arbitrary physical-memory access.
