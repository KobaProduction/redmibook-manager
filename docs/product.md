# Product and interface contract

## Purpose

RedmiBook Manager is an open source local laptop-control application. Initial hardware
target: Xiaomi Redmi Book Pro 16 2024. Windows is the first acceptance platform; Linux
uses the same application contracts and QML interface, with separate OS adapters.
No WebView, browser frontend, cloud account or telemetry is needed for core operation.

The owner specifies product behavior and accepts visible results. Agents implement
small reviewable changes, run checks and supply evidence rather than asking the owner
to program or troubleshoot routine build failures.
Current priority is the Windows installer, installed launch and installer-based
updates. Further Linux environment preparation and GUI/package acceptance are deferred;
retain the source separation without making Linux setup a gate for the owner's Windows preview.

Windows installation is machine-wide under `C:/Program Files/Koba Production/Redmibook Manager`
and requires administrator consent. The GUI runs as a regular user. Future privileged
hardware work belongs to a separate service. Keep writable machine state in ProgramData
and user preferences in the user's application-data directory, outside installed binaries.

## Hello World acceptance

- A packaged native Manager opens overview, battery, Mi-key, settings and diagnostics pages.
- It explicitly says battery/Mi-key functions are not implemented.
- Portable and installed applications run without a development Qt installation on PATH.
- Installer and uninstaller complete; tests and quality gates pass.
- The owner can open the portable build and inspect the window.

The Windows installer registers a diagnostic-only LocalService host and a per-user
sign-in session host. The latter owns OSD independently of the optional tray/Manager.
No key capture, performance control or firmware writes are implemented.

## Subsequent product scope

The first functional iteration defines shared interfaces with Windows/Linux adapters,
machine/session background runtimes, independent OSD and optional tray/settings.
Closing tray/settings must preserve background input and feedback. Follow
[process lifecycle and update acceptance](process-lifecycle.md), including a genuine
payload A-to-B upgrade test. The current scaffold provides diagnostic IPC between hosts.

The first useful controls will be 80% / full-charge protection and user-assigned actions
for the physical Mi key. The accepted process design separates the hardware service,
session runtime and optional GUI, so closing tray/settings must not stop active control or OSD.
Hardware controls remain future acceptance requirements.

Left-clicking the tray toggles a compact dashboard near the icon; right-click opens
the context menu. The dashboard's three performance buttons is an explicitly labelled mock:
it sends no hardware command. Diagnostic feedback defaults to the active display's
top-left work-area corner with a 24-pixel margin and does not take focus. Settings
offers one schematic display with nine position dots for active-display placement and an immediate diagnostic test. Tray sign-in startup defaults
to enabled and can be disabled independently of the service/session. Manager opens
manually. The dashboard shows the base version and Koba Production; its close-tray
action belongs only in the right-click menu.

UI direction: a compact overview, battery settings, Mi-key actions, application/startup
settings and diagnostics. Native controls, keyboard navigation, scalable text and a
shared theme should remain consistent across screens. Unsupported and failed operations
must have honest states; unavailable actions must not look successfully enabled.
The Hello World layout is a technical prototype, not an approved final visual design.
New screen layouts and consequential behavior are shown to the owner before treating
them as an established product contract.

The starting UI languages are English and Russian, with a System option and immediate
switching. Appearance supports System/Light/Dark and follows OS changes in System mode.
Persist per-user choices and apply them consistently to settings, tray and OSD. Follow
[Qt UI conventions](qt-ui-conventions.md); these preferences are implemented with a
versioned per-user store and shared live Qt controllers.

## Development and version ownership

C++20 + Qt 6 / QML + CMake. Follow the dependency rule in `architecture.md`, Serena
requirements in `AGENTS.md`, and executable checks in `development.md`.
Do not add unused DDD patterns or implement unrelated firmware-research work implicitly.

`VERSION.txt` is the single source for application/package base version: **0.1.0** (0.1).
Only an explicit owner instruction may change it. CI must never bump it automatically.
Alpha/beta labels identify release maturity; build identifiers distinguish artifacts
without changing the application base version. See `releasing.md`.

The separate remote `research` branch is not part of this implementation. Do not merge,
delete or replace it without an explicit decision by the owner.

## About and updates

About owns publisher attribution, GitHub link and manual/daily update checks.
The resident session performs at most one persisted automatic check per 24 hours;
users may disable it. User-triggered checks are immediate. Requests use the public
GitHub releases API with a ten-second deadline and bounded response size. No token,
account or telemetry is required. No release/404 is distinct from network failure.

The current alpha channel accepts stable and `x.y.z-alpha.N` releases; the stable
channel excludes prereleases. Compare numeric base version, maturity, then alpha
sequence. CI embeds its workflow run number as MANAGER_RELEASE_SEQUENCE; a local
build has sequence zero. Base product version stays 0.1.0. Search the most recent
100 release records and select the highest supported semantic version.

Choose the exact OS/architecture installer asset (currently Windows x86_64 EXE or
Linux x86_64 DEB). When no matching asset exists, offer the release page instead of
a package for a different architecture. Download opens the validated GitHub asset
in the default browser, only after a user click. This iteration does not run an
installer automatically or promise silent signed updates. A daily notification is
owned by the session and remains independent of the tray.

## Device profile and local licenses

About identifies Koba Production as developer, links the source repository, and
opens complete application/dependency license texts locally. Application source stays
MIT; Qt retains its own license. The Russian MIT text is informational and explicitly
subordinate to the original English text. Do not label all bundled software MIT.

DeviceProfilePort is a framework-independent read port. The current shared Demo
adapter supplies plainly marked sample laptop, BIOS vendor/version, firmware flash
chip and embedded-controller data on both OS compile paths. It does not probe real
hardware or grant any hardware capability. Future native readers and compatibility
policies must distinguish Demo, Unknown and Verified identity. Model/firmware identity
will inform supported feature assembly; a displayed string alone never authorizes a
firmware algorithm or proves that charge control works.
