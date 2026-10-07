# RedmiBook Manager

Open-source tooling for managing and extending RedmiBook laptop functionality across operating systems.

## Initial target

- Xiaomi Redmi Book Pro 16 2024
- Intel Core Ultra 7 155H / Meteor Lake
- Board TM2309

## Project direction

RedmiBook Manager is the operating-system companion to the firmware research project.

Its first responsibility is to expose the hardware-control functions already implemented by the factory platform through documented, recovered contracts. When project firmware is available, Manager should use an explicit firmware capability interface for additional low-level controls instead of relying on hidden vendor behavior.

Primary documents:

- [Manager goals](docs/project-goals.md)
- [Firmware integration contract](docs/firmware-integration.md)
- [Capability inventory](docs/capability-inventory.md)

The firmware project remains the authority for low-level policy and hardware ownership. Manager discovers and uses capabilities that firmware intentionally exposes.

## Core implementation

The first implementation layer is the dependency-free Rust core in `src/`.

It defines:

- semantic capabilities consumed by future UI code;
- TM2309-specific factory WMI packet encoding/response parsing;
- `FactoryFirmwareBackend` typed operations for confirmed controls;
- unit tests for the recovered packet/value mappings.

Raw firmware selectors, EC offsets and GUIDs stay inside the backend/protocol layer. UI code must depend on semantic types such as `PerformanceProfile` instead.

The initial implemented factory slice is deliberately small:

- performance/cooling profile read/write;
- 80% battery charge-protection read/write;
- a semantic CPU-fan-RPM telemetry interface with the concrete Windows PTID transport still pending;

Other capabilities remain represented in the capability inventory and are not promoted to runtime APIs until both their semantic contract and an operating-system-visible transport are proven. A native firmware handler by itself is not sufficient evidence that Windows can invoke the operation.

## Current scope

Factory-backend core implementation is now in progress. The next layers are the platform transport implementation, local execution validation on TM2309, and then the desktop UI.

No replacement firmware is required for the initial Manager feature set; factory-firmware support remains a first-class target.

## Status

Early implementation. The semantic core and confirmed factory protocol slice are being established before UI work.

## Read-only factory probe

The first target execution check is intentionally read-only. On Windows, run the `factory_read_probe` example to discover the active `MICommonInterface` instance and read the confirmed factory-firmware control state without changing platform settings.

The probe reads:

- the semantic performance profile;
- the 80% battery charge-protection state;
- the microphone-mute outward signal as neutral `State0` / `State1` until local execution proof closes its user-facing polarity.

It does not issue any SET request, does not expose arbitrary WMI/ACPI execution, and does not touch the internal native-only selectors that are unavailable through the TM2309 Windows WMAA surface.

GitHub Actions builds the Windows diagnostic executable as the `redmibook-factory-read-probe-windows` artifact. The diagnostic refuses unrelated active WMI instances and reports ambiguity rather than silently choosing a different device. The binary is build-validated in CI; executing it on the actual TM2309 laptop remains a separate acceptance step.
