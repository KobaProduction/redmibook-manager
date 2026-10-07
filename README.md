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

The firmware project remains the authority for low-level policy and hardware ownership. Manager discovers and uses capabilities that firmware intentionally exposes.

## Current scope

Research stage. Current work is driven by recovered factory contracts for performance/fan modes, battery and charging behavior, keyboard/backlight controls, platform events and other TM2309-specific interfaces.

No replacement firmware is required for the initial Manager feature set; factory-firmware support remains a first-class target.

## Status

Research only. No stable public API or supported feature set has been defined yet.
