# Windows installation and data contract

This is an unpackaged native Win32 application with an elevated, machine-wide NSIS
installer. Follow the Microsoft Known Folders model; MSIX/UWP package-private paths
are not the deployment model used here. Microsoft defines the base folders; the
vendor/product subfolders below are our product decision.

| Content | Location | Writer |
| --- | --- | --- |
| Executables, Qt runtime, read-only resources and licenses | Program Files/Koba Production/Redmibook Manager | Elevated installer/updater only |
| User settings, including Mi-key bindings and window preferences | LocalAppData/Koba Production/Redmibook Manager/settings.json | That user's resident session daemon only |
| User logs and cache | LocalAppData/Koba Production/Redmibook Manager/logs and cache | That user's processes |
| Machine charge policy and write journal | ProgramData/Koba Production/Redmibook Manager/service | LocalService, SYSTEM and administrators |
| Future service logs | ProgramData/Koba Production/Redmibook Manager/logs | Authorized service and administrators |
| Temporary files | A unique owned subdirectory of the Windows temporary directory | The creating process |
| Start-menu shortcuts | Common Programs/Redmibook Manager | Elevated installer |

Typical full roots are C:/Program Files, C:/ProgramData and
C:/Users/<user>/AppData/Local. They are examples, not paths to hard-code.
Resolve folders using Windows Known Folder APIs (SHGetKnownFolderPath) or an
equivalent framework API with verified mapping. Use the native 64-bit Program Files
location for this x64 product. Never derive a service path from an interactive user's
profile, the current working directory or the development checkout.

Settings are local because hotkey commands, device choices and hardware capabilities
belong to this computer. Do not put them into Roaming AppData implicitly. If portable
preferences become a product feature, define a separate explicit export/import or
roaming policy. The portable ZIP also uses user data outside its binaries by default;
a truly self-contained writable portable mode needs a separate acceptance decision.

Program Files remains read-only to ordinary users. Do not grant Users/Everyone write
access to it. Setup creates its ProgramData
directories with explicit ACLs for administrators, SYSTEM and its selected service
identity. Ordinary users must not directly overwrite hardware policy, executable paths
or service-trusted input. The GUI requests validated changes through authorized IPC;
it does not acquire administrator rights merely to save its own preferences.

Updates replace program files and preserve user/service data. Settings use
versioned schemas and safe migration. Uninstall must preserve personal settings by
default; any removal of settings must be an explicit user choice and restricted to
owned product subdirectories. Do not recursively delete the shared Koba Production root.

The Windows installer installs binaries, licenses, shortcuts, uninstall registration,
a LocalService machine host and an HKLM Run entry for the user's session host.
The native Manager and tray remain ordinary-user processes. The resident session alone persists preferences using AppLocalDataLocation and
atomic QSaveFile writes. Manager/tray use IPC; installed binaries are never writable data.
The machine host owns the qualified charge/performance command allowlist; the GUI
never acquires administrator rights. TM2309 firmware read/write acceptance succeeded
under LocalService, so no LocalSystem account is required. Setup provisions the service
data directory with a protected inheritable ACL for LocalService, SYSTEM and administrators.
Explicit charge selections persist in charge-policy.v1; charge-write.pending prevents
automatic firmware writes after an interrupted/unconfirmed write, including service restart.
Updates preserve both files. Missing policy means no automatic change; firmware readback
is never adopted as user intent. Ordinary GUI clients cannot modify these files directly.
The same protected directory contains charge-events.jsonl (512 KiB, one previous rotated
file), recording UTC reason and before/desired/after limits/status. The authenticated
read-only charge diagnostic exposes its last flushed entry without opening arbitrary files.
The portable bundle does not register a service or sign-in entry.

## Installer experience

User-facing installation opens the installer executable directly as a visible
English/Russian wizard. Do not substitute a silent PowerShell launcher or a console
window. `/S` is reserved for explicitly requested unattended automation.
The welcome page explains the supported laptop controls, Manager, machine service,
resident hotkey/OSD component and tray, administrator requirement, and preservation
of settings during updates. The progress page shows component shutdown, file copy,
service configuration/check, sign-in registration and restoration. Console helpers
run hidden; their synchronous output is captured in the installation details.
The finish page explains the Start-menu entry, tray mouse actions, sign-in behavior
and independent background lifetimes. Never report completion after a failed service
check or unverified component restart. Do not launch the GUI elevated from setup.

The registered sign-in command starts the resident session with `--with-tray`.
That host reads per-user `ui.trayAutostart` (default true) before opening the tray.
Turning it off affects the next sign-in; it does not stop a running tray or disable
the resident session, OSD or machine service. The main Manager never starts at sign-in.
The Run entry remains registered because it owns the independent session host.
Keep the stable Qt application name/data directory separate from the RedmiBook
display name so presentation branding does not relocate existing user settings.

Registration, SCM start/stop and relaunching the exact sign-in command are separate
acceptance checks from an actual Windows reboot/sign-in. Only mark reboot acceptance
after the owner restarts Windows and the resulting automatic processes/diagnostics
have been inspected; never infer it from the Run entry alone.

Updates use Restart Manager to quiesce owned executable/DLL users and restore prior
clients. An upgrade configures the service without starting an intentionally stopped
or disabled host; an originally running host is restarted and probed. Fresh installation
starts and probes the service. Manifest pruning removes only previously owned obsolete
payload files. User settings survive replacement and uninstall. Payload rollback after
partial file-copy failure is not yet transactional; an error must be reported explicitly.

## Microsoft authorities

- [Known folder identifiers and architecture-specific Program Files mapping](https://learn.microsoft.com/en-us/windows/win32/shell/knownfolderid)
- [Profiles directory: shared ProgramData and local user data](https://learn.microsoft.com/en-us/windows/win32/shell/profiles-directory)
- [Least-privileged applications: separate executable files and writable application data](https://learn.microsoft.com/en-us/windows/win32/dxtecharts/gaming-with-least-privileged-user-accounts)

Checked 2026-10-19. The last article uses older CSIDL terminology; new integrations
should use the Known Folder APIs referenced above.
