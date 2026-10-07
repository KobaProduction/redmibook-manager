# Capability inventory

Target: Xiaomi Redmi Book Pro 16 2024 / TM2309.

This inventory tracks user-facing capabilities rather than raw firmware identifiers. The firmware repository remains the authority for the underlying evidence.

## Readiness states

- **Contract ready** — static target evidence closes the operation and value semantics well enough to implement a factory-firmware backend.
- **Execution pending** — static contract is ready but has not yet been exercised on this exact target through Manager.
- **Partial** — the route exists, but scaling, polarity, provider, or safe write behavior is unresolved.
- **Future firmware** — intended for OpenFirmwareBackend; not available as a confirmed factory-firmware operation.

## Factory-firmware capability matrix

| Capability | Read | Write | State | Notes |
| --- | --- | --- | --- | --- |
| Performance / cooling profile | yes | yes | Contract ready; same-model execution corroborated; local execution pending | Balanced, quiet, performance/Turbo and full-speed profiles are mapped to one canonical platform state. TM2309 runtime testing independently confirms mode application. |
| Battery charge protection | yes | yes | Contract ready; same-model execution corroborated; local execution pending | TM2309 runtime testing independently confirms 80% protection and transition back to 100% when cleared. |
| Adapter-power threshold status | yes | no | Partial | Static threshold behavior is known; physical unit/meaning of the raw source is not fully closed. |
| Microphone-mute platform state | yes | yes | Partial | Static route is closed; outward boolean polarity still needs target execution proof. |
| Keyboard-backlight state | yes/event | no confirmed safe write | Partial | State/event values are known; safe software setter is still unresolved. |
| CPU fan #1 speed | yes | no | Contract ready; Windows transport pending | Intel PTID operating-state element 1 is explicitly labelled CPU Fan #1 Speed, unit RPM. The semantic contract is closed; the preferred Windows user-mode transport still must be selected. |
| CPU fan duty | yes | no | Partial | Value source is known, but scaling is RAW rather than proven percent. |
| Skin temperature 0 | yes | no | Partial | Value source is known, but RAW-to-Celsius conversion is unresolved. |
| Second physical fan speed | unknown | unknown | Partial | Factory Setup displays a GPU-fan field, but no independently proven runtime provider is mapped yet. |
| USB charging mode | setup semantics only | setup semantics only | Partial | Visible Setup values are known; runtime apply path has not been tied to the visible field strongly enough for Manager. |
| USB charging threshold | setup semantics only | setup semantics only | Partial | Visible 10/20/30% values are known; runtime software control path is unresolved. |
| Wake-on-USB / keyboard wake | boot-time firmware state | boot-time firmware state | Partial | Hidden Setup-to-EC synchronization exists; user-facing runtime contract is not closed. |
| Display configuration | yes | yes | Contract ready statically | Native firmware handler maps the factory selector to the confirmed Setup field; user impact still needs execution validation. |
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
- generic MIFS fan RPM, manual fan control and CPU-temperature functions are unavailable on this board;
- CPU fan #1 RPM is exposed by Intel PTID/ACPI, not the MIFS command surface;
- CPU temperature for the desktop UI may use the operating system / Intel thermal stack when that is more reliable than a private firmware route.

The Manager capability model must hide this transport split from the UI.

For Windows, CpuFan1SpeedRpm remains implementation-blocked only on choosing a safe access transport to the PTID method. If the installed Intel PTID provider does not expose a suitable user-mode interface, a minimal read-only privileged helper is allowed. Such a helper must expose named telemetry operations rather than arbitrary ACPI evaluation or arbitrary physical-memory access.
