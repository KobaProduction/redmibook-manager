# RedmiBook Manager goals

## Mission

RedmiBook Manager provides a user-facing operating-system interface to RedmiBook platform controls.

It should support two environments:

1. the unmodified factory firmware, using behavior contracts recovered from the target;
2. project firmware, using a documented capability interface designed for safe low-level control.

The application should not require project firmware for basic features that the factory firmware already exposes.

## Initial feature families

Priority features include:

- cooling and performance profiles;
- fan state and temperature telemetry where the platform exposes them;
- battery charge protection and charging-related controls;
- power and efficiency policy;
- keyboard backlight and hotkey-related controls;
- platform device status;
- firmware and embedded-controller capability reporting.

Future project-firmware capabilities may include:

- richer cooling curves and fan policies;
- undervolting and power-limit tuning where hardware support is proven;
- memory and processor tuning where safely recoverable;
- additional sensor and charging telemetry;
- optional controller configuration for USB, Thunderbolt/USB4 and other platform devices;
- reporting and management of the configured Intel Management Engine policy.

## Safety boundary

Manager is not a generic arbitrary-memory or arbitrary-register editor.

Low-level controls should be exposed as named capabilities with validated ranges and semantics. The firmware decides whether a privileged capability is available.

If firmware disables a function, Manager must report it as unavailable rather than bypassing the firmware policy.

## Compatibility goal

Factory-firmware and project-firmware support should share one user-facing model where the underlying semantics are equivalent.

For example, a cooling profile should have one semantic representation in Manager even if:

- factory firmware implements it through ACPI/WMI and embedded-controller state;
- project firmware later exposes the same operation through a cleaner project-owned interface.

Backend differences must not create duplicate user concepts.


## Current execution priority

Manager is now the primary near-term implementation target.

The work order is:

1. finish recovery of factory-firmware contracts that expose useful hardware functions;
2. define those functions once in the Manager capability model;
3. implement a **FactoryFirmwareBackend** against the confirmed factory interfaces;
4. validate the recovered behavior on the target machine through Manager;
5. later add an **OpenFirmwareBackend** for project firmware without changing the user-facing capability model.

The UI and application logic must depend on semantic capabilities, not directly on WMI selectors, embedded-controller offsets, firmware GUIDs, raw addresses, or other backend-specific identities.

A single user concept such as a performance profile, charge limit, fan mode, or temperature source should remain the same capability regardless of whether its provider is the factory firmware or project firmware.
