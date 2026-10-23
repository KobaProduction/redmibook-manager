# Validation scope

[English](validation.md) · [Русский](validation.ru.md) · [Documentation](README.md)

This describes the acceptance boundary of version 1.1.5, not a guarantee for every
laptop or a substitute for the CI result of a particular commit.

| Area | Evidence and limit |
| --- | --- |
| Windows device | Owner-operated Redmi Book Pro 16 2024, XIAOMI TM2309/V24C1, BIOS RMAMT6B0P0A0A. |
| Charge control | 80/full SET followed by GET qualified; real charging/reconnection exposed retained-policy failures, addressed by the ten-second guard. Long-term cutoff across every dock/charger/suspend scenario is not established. |
| Firmware profiles | Quiet/Balanced/Performance SET/GET and physical Fn+K events confirmed. Fan behavior observed by owner; no CPU-power or frequency guarantee. |
| Display | Internal-panel refresh switching confirmed by owner. External/ambiguous display sources are excluded. |
| Microphone | Owner confirmed software mute lights LED and stops recording, and unmute clears LED/restores recording; endpoint readback checked. Firmware alone was not proven to electrically disconnect audio. |
| Windows energy saver | Native manual on/off qualified on Windows 11 25H2 build 26200.6899. Internal WNF setter is runtime-qualified, not a public Windows compatibility promise. |
| Presets | Contract tests cover storage/migration/localization, preflight, ordering and failure. Isolated UI uses simulated ports; it does not prove every real multi-action sequence or power-plan mutation. |
| Appearance/startup | Application-owned dark/light and Fluent previews; centered spinner motion and asynchronous startup checked. Offscreen results do not prove native GPU/focus behavior. |
| Installation | Repeated different-build local replacements through installer; settings preserved and installed session/core compatibility checked. A same-build reinstall alone is not upgrade evidence. |
| Linux | Shared source, adapters and build/package CI paths prepared. Native hardware control adapters remain unimplemented where stated. No deep Linux desktop/hardware acceptance. |

Windows is the current acceptance platform. Qualification does not extend to another
BIOS just because the model name matches. RPM/fan curves, EC temperature telemetry,
unrestricted charge thresholds and GPU controls are not advertised features.

The full local gate is `verify`: build, formatting, clang-tidy, QML warnings,
architecture/include checks and CTest. CI additionally covers Windows/Linux
Debug/Release, framework-independent core and deployed packages. Consult the actual
[workflow run](https://github.com/KobaProduction/redmibook-manager/actions) for a
published commit; documentation alone never establishes passing CI.

[Firmware evidence](hardware-investigation.md) records qualification chronology.
[Release policy](releasing.md) defines required distribution evidence.
