# Development and quality contract

## Verification cadence

During local implementation, inspect the actual diff and affected behavior. Do not
run full verification, builds, tests, lint, packaging or installation checks after
every edit. Use a targeted check only for a concrete unresolved failure or risk;
do not add checks of checks or tests that mirror trivial implementation details.

Run the final full `verify` once immediately before an owner-authorized commit,
after local UI review is finished. Reuse passing results for an unchanged revision.
After a change or failure, repeat only the checks it invalidates. Publishing that
same verified revision does not require another local full pass. CI remains
independent, and quality gates must not be disabled.

## Toolchain

For an isolated build-tree Windows core/session/tray/Manager loop without
installation or packaging, use [the development runtime](dev-runtime.md).

Qt is pinned to 6.10.2 in CI; C++20, GCC 13 (MinGW 13.1 on Windows), LLVM 19,
CMake 3.30+ and Ninja are the baseline. Python is used only as a standard-library
runner for vendored checkdeps and its regression tests; this is not a Python product.
Windows CI installs version-pinned LLVM binary wheels from `tools/requirements-quality.txt`;
the application has no Python runtime dependency. This small tool-only requirements
file deliberately does not introduce a Python application/package environment.
CI pins action revisions and aqt 3.3.0. System packages come from the named runner
distribution; this is not a hermetic toolchain or bit-reproducible binary guarantee.

## Gates

Run from the checkout root with the toolchain on PATH and `QT_ROOT` set.

```sh
cd <checkout>
cmake --workflow --preset verify
```

Individual CMake targets are `format-check`, `format`, `lint`, `boundaries`,
`qml-strict` (zero QML warnings), Qt’s `all_qmllint`, and aggregate `quality`. `ctest --preset release` runs tests.
The C++ build itself is the type check. Debug has independent configure/build/test presets.
Formatting changes C++ files only; QML conventions are checked with qmllint.

- Compiler warnings are errors for owned C++ targets.
- clang-format 19 defines formatting.
- clang-tidy uses the actual compilation database and blocks on diagnostics.
  Maximum 20 statements and cognitive complexity 15; no physical-line limit.
- qmllint checks registered QML types after generation.
- Chromium checkdeps checks forbidden literal includes, including untracked source.
  It does not resolve macros, arbitrary flat include paths, or semantic dependencies.
  CMake, compilation and architecture review remain necessary.
- Contract tests verify platform propagation; offscreen smoke creates the QML window,
  checks the product window title and exits with a bounded CTest timeout.
- CI deploys and starts the installed bundle separately from the build tree.

Use `BUILD_TESTING=ON` for the required quality gate. Do not suppress warnings,
disable hooks, use a historical baseline, or move logic into macros to evade limits.

## Git hooks

Install the tracked hook on every new checkout:

```sh
cd <checkout>
cmake -P tools/install-hooks.cmake
git config --get core.hooksPath
```

The result must be `.githooks`; `pre-push` invokes the full verify workflow and
propagates its failure. Windows uses PowerShell 7 with `tools/dev.ps1`; Linux uses
CMake directly. The caller supplies `QT_ROOT`; no machine-specific configuration
is committed. Local hooks can be bypassed outside this contract, so server CI is
independent. Branch protection is a separate repository setting, not a YAML property.

On Windows, `tools/dev.ps1 -Action verify` records a successful result in the Git
metadata directory for the exact staged tree and toolchain locations. Stage the
complete change before the final verification. Pre-push reuses that result only
when the index and working files still match and no untracked source remains.
Changed inputs invalidate the receipt; a failed run never records success. CI
always runs its own checks independently.

## Packaging and acceptance

Follow [the Windows installation and data contract](windows-installation.md) for
standard directories, update persistence and service permissions.

CPack installs into a staging tree and Qt's QML deployment script collects runtime
libraries/plugins/imports. Windows ZIP is portable; the NSIS installer is machine-wide
and provides uninstall. Linux TGZ is a bundle; DEB installs the same app plus a desktop entry.
Windows setup registers the qualified firmware service and session sign-in entry; extracting
the portable ZIP does neither. Qt LGPL/GPL notices and license
files must accompany deployment; application code remains MIT.

Windows default install directory is `C:/Program Files/Koba Production/Redmibook Manager`
(the system's 64-bit Program Files location). Installation and updates require UAC elevation.
The installed executable is `bin/redmibook-manager.exe`; Qt DLLs and plugins are
deployed within that installation, with no dependency on the development checkout.
NSIS registers 64-bit machine uninstall and all-user Start-menu shortcuts. Launch the
GUI normally from its shortcut; it does not require administrative rights. Re-running
the installer updates the existing install
directory without changing the base version. Restart Manager quiesces owned clients
and the machine service before replacement and restores prior running components.
If a component cannot stop, setup fails before replacement. This is installer-based updating,
separate from the release checker, which opens downloads only after a user click.
The pinned CMake NSIS template in `packaging/windows` fixes initialization to the
all-user, 64-bit registry context and remembers the machine install directory while
preserving explicit test /D overrides. Its provenance and license are included.

CI validates the installed Windows/Linux app with a bounded smoke run. A package
test must remove development Qt/MinGW paths, clear QML/plugin overrides, use an
unrelated working directory and execute the deployed app. Save a screenshot with
`--smoke-test --screenshot <path>` for visual inspection. Offscreen software rendering
does not establish GPU, native desktop, suspend/resume or hardware acceptance.

On Windows, `tools/verify-windows-package.ps1` automates ZIP extraction into a path
with spaces, dependency/license checks, sanitized launch, screenshot capture and
isolated install/run/uninstall in elevated 64-bit PowerShell. It refuses to replace an existing machine installation;
use `-SkipInstaller` to preserve that installation. It leaves a portable preview in
`dist/preview/`, an image and a local `verification.json`. `tools/dev.ps1 -Action run`
opens that preview for the owner. Test processes are bounded and cleaned up.
For reproducible offscreen text rendering on Windows, the test uses system fonts and
Fusion controls; this is test-only configuration, not a change to the native UI theme.

Use a short checkout path on Windows: MinGW and generated Qt resource object paths
can still exceed toolchain path limits in deeply nested workspaces.

For published binaries, validate download/install/uninstall on supported clean OS
images and configure signing before claiming trusted distribution. The Hello World
bootstrap may produce explicitly unsigned development packages.

The CI package verifier checks install, same-build reinstall and uninstall. It reports
that scope explicitly; this does not prove a different-build A-to-B upgrade. A-to-B
acceptance additionally verifies build/hash replacement, stale payload pruning,
service restart and preserved preferences as specified in process-lifecycle.md.
