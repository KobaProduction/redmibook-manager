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

Other capabilities remain represented in the capability inventory and are not promoted to runtime APIs until both their semantic contract and an operating-system-visible transport are proven. A native firmware handler by itself is not sufficient evidence that Windows can invoke the operation.

## Current scope

Factory-backend core implementation is now in progress. The next layers are the platform transport implementation, local execution validation on TM2309, and then the desktop UI.

No replacement firmware is required for the initial Manager feature set; factory-firmware support remains a first-class target.

## Status

Early implementation. The semantic core and confirmed factory protocol slice are being established before UI work.
