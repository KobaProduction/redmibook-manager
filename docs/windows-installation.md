# Windows installation and data contract

This is an unpackaged native Win32 application with an elevated, machine-wide NSIS
installer. Follow the Microsoft Known Folders model; MSIX/UWP package-private paths
are not the deployment model used here. Microsoft defines the base folders; the
vendor/product subfolders below are our product decision.

| Content | Location | Writer |
| --- | --- | --- |
| Executables, Qt runtime, read-only resources and licenses | Program Files/Koba Production/Redmibook Manager | Elevated installer/updater only |
| User settings, including Mi-key bindings and window preferences | LocalAppData/Koba Production/Redmibook Manager/settings.json | That user's GUI/session agent |
| User logs and cache | LocalAppData/Koba Production/Redmibook Manager/logs and cache | That user's processes |
| Future machine hardware policy and service state | ProgramData/Koba Production/Redmibook Manager/service | Authorized service and administrators |
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
access to it. When machine policy storage is implemented, setup must create its ProgramData
directories with explicit ACLs for administrators, SYSTEM and its selected service
identity. Ordinary users must not directly overwrite hardware policy, executable paths
or service-trusted input. The GUI requests validated changes through authorized IPC;
it does not acquire administrator rights merely to save its own preferences.

Updates replace program files and preserve user/service data. Future settings need
versioned schemas and safe migration. Uninstall must preserve personal settings by
default; any removal of settings must be an explicit user choice and restricted to
owned product subdirectories. Do not recursively delete the shared Koba Production root.

The Windows scaffold installs binaries, licenses, shortcuts, uninstall registration,
a diagnostic-only LocalService host and an HKLM Run entry for the user's session host.
The native Manager and tray remain ordinary-user processes. Preferences use Qt's
AppLocalDataLocation, QSaveFile and QLockFile; installed binaries are never writable data.
No machine policy store or ProgramData ACL provisioning is needed by the diagnostic host.
The portable bundle does not register a service or sign-in entry.

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
