# Built-in NFC tag

The NFC feature belongs to the resident user session. Manager is an IPC client;
closing Manager does not stop observation. The shared application port and NDEF
format live in `application/nfc`, Type 5/NTAG handling in `adapters/nfc`, native
transports in `adapters/{windows,linux}/nfc`, and presentation in
`presentation/nfc_model.*` and `presentation/qml/pages/NfcCard.qml`.
`manager_nfc_application` contains only standard C++ message/draft rules and
the checked-write use case. `manager_nfc` supplies native tag adapters and is
assembled in the session, never linked by the presentation target. The IPC
adapter invokes the application write use case on the session worker.

## Capability and product boundary

The qualified RedmiBook module is NXP NTAG5 link (ACPI `NTAG5332`), a dual-interface
NFC Forum Type 5 **tag** with an I2C host interface. A phone acting as a reader can
read its NDEF records and supply the RF field observed by the laptop. It is not
an NFC reader: presenting another passive tag or bank/access card does not let
the laptop read that card. Field detection identifies an active NFC reader nearby,
not a particular phone, identity or completed phone transaction.

Manager shows the feature only when an actual module is discovered. Present but
denied, busy, incompatible or failed modules retain their diagnostic state. A
missing module contributes no card. There are no OS-name branches in QML.

The NFC card shows the module model, status, nearby NFC field and occupied memory.
The heading places NFC and its module model on one row; state and memory share
a compact row, with wrapping on narrow cards. The footer's Records button opens
a nested record editor with add, edit/replace, remove,
import and export actions. Text, URI, telephone and email templates are available;
telephone and email create standard URI records. Existing custom/MIME/vendor records
retain their type, ID and payload when other records are edited. The list supports
adding, editing/replacing and deleting records. Changes stay in the editor's draft
until the owner confirms a complete write. It does not implement phone pairing,
file transfer, screen mirroring, credential cloning, security configuration or
tag emulation. Factory settings, capability container, protection registers and
RF-only counter are outside the write allowlist.

The original message observed on the owner's laptop contains the external record
`com.xiaomi.mi_connect_service:externaltype`. It is ordinary editable tag content,
not a required system record or a dependency of Manager. Xiaomi documents the
touchpad tap as an entry point for its phone-mirroring/file-transfer software
([PC Manager help](https://pc.mi.com/pc-manager-help)); the complete proprietary
payload format has not been decoded. Keeping existing records during an add/edit
operation is a generic preservation rule. The owner may remove this record or
clear the message by removing its records. No vendor record is protected specially.

## Ownership and command contract

Native calls run on a serialized session worker. Native notifications invalidate
the snapshot; the existing shared ten-second reconciliation recovers missed
notifications. Background reads do not set command busy state. Snapshots publish
only actual changes. There is no encounter counter or page-owned polling timer.

The per-user `.Nfc.v1` local endpoint checks schema, product version and payload
build. It bounds messages, clients and NDEF data. Offline and incompatible clients
cannot write. There is no preference-file fallback or second settings authority.

The record editor captures the expected NDEF when an editing session starts. Later observations never
silently rebase an open draft. A write includes this expected NDEF. The session
rereads it and rejects stale content before touching hardware. The adapter rereads and rejects stale content,
active RF fields, protection, invalid NDEF and capacity overflow. It invalidates
the NDEF length, stages changed user-memory pages, then commits the length last.
Each programmed page and the resulting NDEF are read back. Physical removal or
power loss cannot be made atomic; an unconfirmed write remains an explicit unknown
outcome. No automatic write retry occurs. The session retains the native job past
the UI deadline and rejects overlapping commands until it completes; a subsequent
authoritative read is required for reconciliation.

Manager displays decoded URI/text records inside the nested record editor. The editor
starts from the current complete message, retaining opaque records. Cancellation
does not write. The encoded draft size is checked against the adapter's exact
`max_content_size`, including short/extended TLV length and any retained terminator.
Field-only observations do not reset or repopulate the editor. A changed message
invalidates its captured expectation; drafts are never silently rebased.
The framework-independent `application/nfc/Draft` owns record mutation, opaque
record/ID preservation, cancellation, conflict detection and write preparation.
The Qt model translates input and results and exposes notifications; QML owns
only presentation and modal navigation. There is no parallel single-record write API.

Record order matters: Android dispatch uses the first record to select handling;
storing several records does not promise that the phone will execute all of them.
See [Android NDEF dispatch](https://developer.android.com/develop/connectivity/nfc/nfc).

The editor places memory use beside its title, Add record below the list, and
import/export beside the closing actions in its footer. The list editor,
individual record form and write confirmation are modal. There is no
Details dialog, raw-byte viewer/editor, overflow action menu, backup browser or
restore-original action. Import loads a standard `.ndef` file into the draft;
export saves the draft when editing, otherwise the observed complete NDEF. Neither
action writes hardware. The session has no NFC archive endpoint and creates no
automatic copies. Previously created backup files are left on disk unchanged.
Import/export use the injected `FilePort` and `adapters/storage/nfc_files` worker
adapter. The UI never performs file I/O. File requests are capped at 2048 bytes,
serialized per adapter and have a ten-second UI deadline. A timed-out native file
operation remains owned until completion; it cannot apply a late import to a closed
or different draft, and no automatic retry occurs. Atomic export uses QSaveFile.

NXP EEPROM access NACKs while `EEPROM_WR_BUSY` is set; session-register access is
still available and the chip provides no EEPROM-ready interrupt. Only an explicit
write uses bounded, worker-side status transactions (64 maximum and a 20 ms
deadline per programmed page), followed by data readback. This protocol requirement
does not introduce a background polling loop or fixed-delay retry.

## Platform qualification

Windows uses the installed NXP SPB driver through cancellable overlapped I/O.
The private ABI is enabled only for the inspected `NTAG5Driver.sys` SHA-256
`EAC8E8BE0DE560119E7A9862DE7D343BF7B9555AA27C322E89BC473E852EF180`.
A different binary reports incompatible rather than receiving guessed commands.
The application requires the existing OEM NXP `NTAG5Driver.sys`; it neither ships
nor installs a kernel driver. The application installer updates Manager/session/core
only. A missing driver file has a distinct `DriverMissing` status while the detected
module remains visible for diagnostics. Native device-open and I/O failures remain
separate from absence of the NFC module. The user session uses the reverse-engineered
driver ABI through `DeviceIoControl`/`WriteFile`; the driver owns I2C/controller access.
The device is opened exclusively without elevation. Interrupt observation temporarily
sets the volatile session ED mode and verifies it; shutdown restores the observed
previous mode. Persistent EEPROM configuration is not changed.

Linux uses `i2c-dev` only for an identified NTAG5332 sysfs client. It never scans
arbitrary addresses, forces a busy slave or unbinds a kernel driver. A numeric
I2C client name supplies its address; ACPI-named clients additionally require the
qualified TM2309 / RMAMT6B0P0A0A DSDT mapping to address `0x54`. Other mappings
report incompatible. Normal `/dev/i2c-*` permissions are required; the application
does not elevate itself or install broad device permissions. This adapter has no
userspace GPIO interrupt path: field observation is periodic and can miss short
taps between reconciliations. A native Linux hardware boot and RF
interaction are separate from compilation and WSL absence checks.

## Validation and sources

`nfc-tag-contract` covers NDEF/Type 5 parsing, EEPROM preservation, stale/active-field
rejection and unconfirmed writes. `nfc-session-contract` covers asynchronous IPC,
compatibility, bounded input and stale-content rejection. `nfc-preview` renders the
actual QML card offscreen without accessing hardware. `nfc-draft-contract` checks
the standard-C++ draft/write boundary independently of Qt and platform adapters.
`nfc-composer-contract` includes asynchronous file round trips and the model's
preservation/conflict behavior. `nfc-probe` is an explicit
hardware diagnostic, excluded from automated hardware-independent CTest runs.
Its `--roundtrip <backup.ndef>` action performs a reversible test write and restores
the original valid NDEF; `--events` checks subscription and cancellation.

On the qualified Windows laptop, native reading, confirmed write/restore and
interrupt subscription/cancellation passed. The complete readable user-memory
image matched its original SHA-256 after restoration. The owner subsequently
confirmed that physical phone approaches were detected and the phone opened the
written URI. Native Linux hardware acceptance remains separate.

Protocol authority: [NXP NTP53x2 data sheet](https://www.nxp.com/docs/en/data-sheet/NTP53x2.pdf)
and [NTAG5 I2C interface application note](https://www.nxp.com/docs/en/application-note/AN12364.pdf).
The private Windows IOCTL ABI was recovered from the installed, hashed driver;
Microsoft's sample SPB IOCTL numbering is not interchangeable with it.
