# RedmiBook Manager

[English](README.md) · [Русский](README.ru.md)

Native laptop controls for **Xiaomi Redmi Book Pro 16 2024**, built with C++20,
Qt 6.10.2 and QML. Version **1.1.5**. Windows is the tested platform; Linux has
build/package infrastructure but has not received deep desktop or hardware acceptance.
No WebView, cloud account or usage telemetry is required.

## Features

- **Battery:** 80% / full-charge policy, charge/discharge rate in W, capacity in Wh,
  voltage and estimated wear. A ten-second guard detects charging at or above 80%
  and reconciles the selected protection policy.
- **Firmware performance:** Quiet, Balanced and Performance profiles, with observed
  state shared by Manager, tray and OSD. These are vendor firmware profiles, not
  promises of fixed CPU clocks, voltage or power consumption.
- **Built-in display:** select supported refresh rates and choose which rates
  participate in shortcut/tray cycling. External displays are not changed.
- **Microphone:** qualified firmware flag/keyboard LED control followed by Windows
  endpoint mute reconciliation; drivers and devices remain enabled.
- **Hotkeys:** configurable vendor-key actions, including Open Manager, Open tray
  and Do nothing; Fn+S refresh action; firmware-owned Fn+K remains fixed.
- **Power presets:** ordered combinations of firmware profile, internal-display
  refresh, Windows power scheme, manual energy saver and brightness. Editable
  Economy/Performance starter buttons are included, hidden from the tray by default.
- **Tray:** compact dashboard, configurable status elements and preset ordering;
  left-click opens the dashboard, right-click opens the menu.
- **Feedback and appearance:** independent OSD and sound switches; Fluent controls,
  System/Light/Dark themes and live English/Russian switching. Factory preset
  captions translate; user-entered names remain unchanged.
- **Settings and updates:** daemon-owned preferences, reset to defaults, manual and
  optional daily update checks. Reset restores full charging without changing the
  current screen mode. Downloading an update requires a user action.

## Screenshots

Application-owned, isolated previews with simulated device data; these are not
hardware measurements. More views: [gallery](docs/screenshots.md).

![Tray dashboard](docs/images/tray-dark.png)

<details>
<summary>Overview, device controls and light theme</summary>

![Overview](docs/images/overview-dark.png)
![Device controls](docs/images/device-dark.png)
![Light theme](docs/images/overview-light.png)

</details>

## Compatibility and installation

Firmware writes are qualified for **TM2309 / RMAMT6B0P0A0A** only. Another RedmiBook
model or BIOS version is not automatically compatible. Unsupported features are
omitted from the capability catalog; a failed operation is reported separately.
Firmware settings and actual battery charging behavior remain distinct observations.

Download the matching package from [Releases](https://github.com/KobaProduction/redmibook-manager/releases).
Windows x86_64 provides an NSIS installer and portable ZIP; Linux x86_64 provides
DEB and tar.gz builds. Development packages are unsigned alpha builds.

On Windows, run the installer and approve UAC. It installs to
`Program Files/Koba Production/Redmibook Manager`, registers the machine service
and resident user-session component, and preserves settings during upgrades.
Open Manager from the Start menu. The GUI runs without administrator rights.
Closing Manager or the tray leaves hotkeys, feedback and charge protection running.
Portable extraction alone does not register those background components.

See the [user guide](docs/user-guide.md) and [validation scope](docs/validation.md).

## Architecture and extension

`apps → presentation / adapters → application → domain`

Domain/application use standard C++. OS APIs, persistence and IPC live in adapters;
composition roots assemble only supported device/OS capabilities. The resident
session daemon owns user settings in RAM and publishes revisions over IPC. The
machine service owns privileged firmware policies. Manager and tray are clients,
not settings-file writers or privileged hardware controllers.

Native work runs asynchronously outside the GUI thread. Notifications drive state
updates; bounded ten-second reconciliation recovers missed events. Opening battery
details temporarily requests 100 ms, 1 sec or 5 sec sampling; closing the view
releases it. New BIOS/model support requires explicit identity qualification,
read/write verification and an adapter, not a model-name check in QML.

[Architecture and extension](docs/architecture.md) ·
[Component contracts](docs/component-contracts.md) ·
[Firmware evidence](docs/hardware-investigation.md)

## Build and verify

Requirements: C++20 compiler (MinGW 13.1 on Windows), Qt **6.10.2**, CMake **3.30+**,
Ninja, Python **3.12+**, clang-format/clang-tidy **19**; PowerShell **7** on Windows
and NSIS **3.11** for its installer. Set `QT_ROOT` to the Qt desktop kit directory.
The [CI workflow](.github/workflows/ci.yml) lists Linux dependencies.

```powershell
# Windows
cd <checkout>
cmake -P tools/install-hooks.cmake
./tools/dev.ps1 -Action verify -QtRoot $env:QT_ROOT
# Package the verified build without repeating verify:
cpack --config build/release/CPackConfig.cmake
```

```sh
# Linux, Qt/compiler/LLVM tools on PATH and QT_ROOT set
cd <checkout>
cmake -P tools/install-hooks.cmake
cmake --workflow --preset verify
cpack --config build/release/CPackConfig.cmake
```

`cmake --build --preset release` builds the desktop app. The `core` configure,
build and test presets exercise the inner layers without Qt. Use focused checks
during development and one full `verify` before a checkpoint commit. CI separately
builds/tests Windows and Linux in Debug/Release and checks deployed packages.
Pushes to `main` build artifacts; `release` publishes an alpha only after the matrix
passes. See [development](docs/development.md), [preview runtime](docs/dev-runtime.md)
and [release policy](docs/releasing.md).

[Documentation index](docs/README.md) · [MIT license](LICENSE) ·
[Third-party notices](packaging/THIRD_PARTY_NOTICES.md) ·
[Issues](https://github.com/KobaProduction/redmibook-manager/issues)
