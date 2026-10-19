# RedmiBook Manager

Open source native laptop manager for Windows and Linux, built with C++20 and Qt Quick.
The current version is a diagnostic application scaffold. Windows setup installs
a diagnostic-only machine service and registers the session runtime at sign-in.
Charge control, actual Mi-key capture and performance switching are not implemented.

Open Redmibook Manager from its Start-menu shortcut. Overview and Diagnostics show
background connectivity; Settings changes English/Russian language and System/Light/Dark
appearance live. Left-click the tray icon for its mini-dashboard; right-click for its
menu. Performance selection there is explicitly a mock and never changes the laptop.
Diagnostic signals show an independent OSD on the selected display (active display by default). Closing Manager or choosing
Close tray only leaves the session runtime and service running. Restart background
components requests administrator consent and restarts the product, not Windows.
Tray startup at sign-in is enabled by default and can be disabled in Settings;
the main Manager opens manually. Settings provides a graphical display/position picker and a Test indicator button. The service/session stay independent.
Manager uses Qt Fluent controls by default with a Classic alternative; color scheme
and interface design are separate preferences. About provides the GitHub link,
manual update checks and optional daily notifications. Downloads open in the default
browser after a user click; no installer runs automatically.

Windows defaults to `Program Files/Koba Production/Redmibook Manager`; per-user settings
live in `LocalAppData/Koba Production/Redmibook Manager/settings.json`. Linux retains
compile paths and session/UI boundaries; its native machine service is unavailable.

## Windows development

Install Qt **6.10.2 MinGW 64-bit**, MinGW **13.1**, CMake **3.30+**, Ninja,
Python **3.12+**, PowerShell **7**, and LLVM **19** (clang-format/clang-tidy).
Qt Creator's bundled LLVM 19 tools work. For the installer, install NSIS **3.11**
and add its directory to PATH.
Set `QT_ROOT` to the Qt `mingw_64` directory. `QT_TOOLS` can override the sibling
Qt Tools directory. No personal paths belong in tracked configuration.

From the repository root:

```powershell
cd <checkout>
cmake -P tools/install-hooks.cmake
./tools/dev.ps1 -Action verify -QtRoot <Qt-directory>/6.10.2/mingw_64
./tools/dev.ps1 -Action package -QtRoot <Qt-directory>/6.10.2/mingw_64
./tools/dev.ps1 -Action package-test -QtRoot <Qt-directory>/6.10.2/mingw_64
./tools/dev.ps1 -Action run -QtRoot <Qt-directory>/6.10.2/mingw_64
```

`tools/dev.ps1` prepares the Qt/MinGW tool paths for that invocation. An installed
pre-push hook requires `QT_ROOT` in the calling environment. Run the application
from the extracted portable ZIP (`bin/redmibook-manager.exe`), or run from the build
directory with Qt and MinGW `bin` directories on PATH.

## Linux development

Install GCC with C++20 support, Qt **6.10.2** desktop development files, CMake
**3.30+**, Ninja, Python **3.12+**, LLVM **19** and Qt's platform dependencies.
Set `QT_ROOT` to the Qt `gcc_64` directory; put its `bin` and LLVM tools on PATH.
The CI workflow documents Ubuntu system packages and the pinned Qt installation.

```sh
cd <checkout>
cmake -P tools/install-hooks.cmake
cmake --workflow --preset verify
cpack --config build/release/CPackConfig.cmake
```

Build the GUI with `cmake --build --preset release`; run `build/release/bin/redmibook-manager`.
The independent core can be built/tested without Qt using the `core` configure,
build and test presets; this validates domain/application and the native OS adapter.
Release packages go to `dist/`: Windows ZIP + NSIS installer, Linux tar.gz + Debian package.
The Linux tar bundle is relocatable on compatible Linux distributions; it does not
bundle glibc or promise compatibility with older distributions. Windows packages
are unsigned until a signing identity is configured.

## Checks and CI

`verify` configures, builds, checks formatting, static analysis, QML and architecture,
then runs CTest including the bounded UI smoke test. See [development](docs/development.md)
for separate targets, hook installation and packaging verification.

The prepared GitHub Actions workflow runs Windows/Linux Debug and Release builds,
quality checks, tests, deployment and package checks after it is published.
Pushes to `main` build artifacts; promotion to `release` publishes an alpha prerelease
after successful checks. Application base version stays **0.1.0** until the owner
explicitly changes it. [Release policy](docs/releasing.md) defines immutable build labels.
Current owner gate: all work stays local, including no commits, until personal app acceptance.
No certificate, credential, or production connection is needed for normal CI.

[Architecture](docs/architecture.md) · [Agent instructions](AGENTS.md) ·
[Product/UI contract](docs/product.md) ·
[MIT license](LICENSE) · [Issues](https://github.com/KobaProduction/redmibook-manager/issues)
