# Documentation

[English README](../README.md) · [Русский README](../README.ru.md)

## Users

| Topic | English | Русский |
| --- | --- | --- |
| Installation, controls, presets and settings | [User guide](user-guide.md) | [Руководство](user-guide.ru.md) |
| Tested capabilities and acceptance limits | [Validation](validation.md) | [Что проверено](validation.ru.md) |
| Screenshots / скриншоты | [Gallery](screenshots.md) | [Галерея](screenshots.md) |

## Contributors

Technical contracts are maintained in English as the shared source of truth.
Пользовательская документация доступна на двух языках; технические контракты
ведутся на английском, чтобы не расходились две архитектурные спецификации.

- [Architecture and extension points](architecture.md): source navigation and adding OS/device adapters.
- [Product contract](product.md): implemented behavior, ownership and limits.
- [Module layout](module-layout.md): target architecture blueprint, distinct from today's file layout.
- [Component contracts](component-contracts.md), [process lifetimes](process-lifecycle.md),
  [daemon preferences](preference-authority.md): IPC, state and authority.
- [Tray presets](tray-presets.md), [Windows power controls](windows-power-controls.md),
  [Qt UI conventions](qt-ui-conventions.md): feature and presentation contracts.
- [Firmware investigation](hardware-investigation.md): evidence, exact qualified identity and native operations.
- [Development/quality](development.md), [isolated previews](dev-runtime.md),
  [Windows installation](windows-installation.md), [releases](releasing.md).

Implementation details belong to source. Mutable bugs and feature requests belong
in [Issues](https://github.com/KobaProduction/redmibook-manager/issues).
