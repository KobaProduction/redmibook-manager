# Firmware integration contract

## Responsibility split

### Firmware owns

- early platform initialization;
- hardware safety limits;
- embedded-controller policy;
- privileged platform state;
- boot-time defaults;
- availability of low-level capabilities;
- Management Engine policy;
- any interface that must remain trustworthy before the operating system starts.

### RedmiBook Manager owns

- capability discovery;
- user-facing configuration;
- presentation of telemetry;
- persistence of user preferences when appropriate;
- translation between user concepts and documented firmware operations;
- clear reporting when an operation requires reboot, firmware Setup, or is unavailable.

Manager must not silently override firmware policy.

## Factory firmware backend

The initial backend should use recovered factory interfaces when they are CONFIRMED.

Current examples include:

- performance/fan profile through the recovered WMI/embedded-controller contract;
- battery charge-protection state;
- keyboard/backlight-related platform events;
- other ACPI/WMI controls as their behavior contracts are closed.

Raw firmware identifiers remain backend details. The Manager API should expose semantic capabilities.

Factory firmware is intentionally multi-transport. WMI is the confirmed runtime control transport for the supported TM2309 profile/battery paths, while CPU fan RPM is a separate Intel PTID/ACPI telemetry contract. The semantic core therefore separates control operations from cooling telemetry instead of forcing both through one raw transport.

For Windows PTID access, prefer an already-installed Intel provider when it exposes the required value safely. If no suitable provider surface exists, a project helper may be used only as a scoped read-only device-stack component for the confirmed PTID telemetry operation. It must not expose arbitrary ACPI method evaluation, arbitrary physical-memory access, or a generic privileged command channel to the desktop application.

## Project firmware backend

Future project firmware should expose an explicit versioned capability interface.

The exact transport is not fixed yet. It may use standard ACPI methods, UEFI variables, a dedicated device/interface, or another mechanism selected after the threat model is understood.

The interface should support:

- protocol version and firmware identity;
- capability enumeration;
- typed reads and writes;
- ranges/enumerations for configurable values;
- readback after writes;
- clear unsupported/locked states;
- firmware-side validation of privileged values.

Do not invent encryption or authentication merely for obscurity. If authentication is needed, define it from the actual threat model and privilege boundary.

## Policy model

A capability can be:

- supported and writable;
- supported but read-only;
- available only before operating-system handoff;
- available only after reboot or Setup change;
- locked by firmware policy;
- unavailable on this hardware/firmware.

Manager must distinguish these states.

This is particularly important for Intel Management Engine policy. The firmware may intentionally remove normal operating-system access to that subsystem. Manager must continue to function and should report the active policy rather than assuming the interface exists.

## Feature model

The shared semantic model should cover, as evidence allows:

- cooling profile;
- fan mode/curve/control;
- temperatures and fan speeds;
- performance/power profile;
- battery charge protection and charging thresholds;
- keyboard/backlight modes;
- platform hotkeys and events;
- processor power/voltage controls;
- memory tuning;
- optional platform controller enable/disable/configuration;
- low-level telemetry intentionally exported by project firmware;
- Intel Management Engine policy state.

Each capability must carry its support state and constraints.

## Cross-repository contract

The firmware repository is the source of truth for recovered low-level behavior and project-firmware capability definitions.

The Manager repository consumes those contracts and defines the user-facing API/UI.

A low-level control should not be duplicated independently in both repositories. When a firmware contract changes, Manager compatibility should be updated against the versioned semantic contract rather than raw addresses or undocumented implementation details.
