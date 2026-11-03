# Built-in NFC tag

The NFC feature belongs to the resident user session. Manager is an IPC client;
closing Manager does not stop observation. The shared application port and NDEF
format live in `application/nfc`, Type 5/NTAG handling in `adapters/nfc`, native
transports in `adapters/{windows,linux}/nfc`, and presentation in
`presentation/nfc_model.*` and `presentation/qml/pages/NfcCard.qml`.

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

The feature reads the built-in tag's NDEF, exports/imports `.ndef` files and writes
text, HTTP(S) links or validated raw NDEF. It does not implement phone pairing,
file transfer, screen mirroring, credential cloning, security configuration or
tag emulation. Factory settings, capability container, protection registers and
RF-only counter are outside the write allowlist.

## Ownership and command contract

Native calls run on a serialized session worker. Native notifications invalidate
the snapshot; the existing shared ten-second reconciliation recovers missed
notifications. Background reads do not set command busy state. Snapshots publish
only actual changes. An encounter count is session-local observed rising field
edges; it is not a durable transaction history.

The per-user `.Nfc.v1` local endpoint checks schema, product version and payload
build. It bounds messages, clients and NDEF data. Offline and incompatible clients
cannot write. There is no preference-file fallback or second settings authority.

A write includes the expected current NDEF. The session durably saves that NDEF
under its SHA-256 filename in `nfc/backups` beneath the session's application data
directory before touching hardware. The adapter rereads and rejects stale content,
active RF fields, protection, invalid NDEF and capacity overflow. It invalidates
the NDEF length, stages changed user-memory pages, then commits the length last.
Each programmed page and the resulting NDEF are read back. Physical removal or
power loss cannot be made atomic; an unconfirmed write remains an explicit unknown
outcome. No automatic write retry occurs. The session retains the native job past
the UI deadline and rejects overlapping commands until it completes; a subsequent
authoritative read is required for reconciliation.

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
The device is opened exclusively without elevation. Interrupt observation temporarily
sets the volatile session ED mode and verifies it; shutdown restores the observed
previous mode. Persistent EEPROM configuration is not changed.

Linux uses `i2c-dev` only for an identified NTAG5332 sysfs client. It never scans
arbitrary addresses, forces a busy slave or unbinds a kernel driver. A numeric
I2C client name supplies its address; ACPI-named clients additionally require the
qualified TM2309 / RMAMT6B0P0A0A DSDT mapping to address `0x54`. Other mappings
report incompatible. Normal `/dev/i2c-*` permissions are required; the application
does not elevate itself or install broad device permissions. This adapter has no
userspace GPIO interrupt path: the UI labels periodic field observation, which
can miss short taps between reconciliations. A native Linux hardware boot and RF
interaction are separate from compilation and WSL absence checks.

## Validation and sources

`nfc-tag-contract` covers NDEF/Type 5 parsing, EEPROM preservation, stale/active-field
rejection and unconfirmed writes. `nfc-session-contract` covers asynchronous IPC,
compatibility, bounded input and backup-before-write. `nfc-preview` renders the
actual QML card offscreen without accessing hardware. `nfc-probe` is an explicit
hardware diagnostic, excluded from automated hardware-independent CTest runs.
Its `--roundtrip <backup.ndef>` action performs a reversible test write and restores
the original valid NDEF; `--events` checks subscription and cancellation.

On the qualified Windows laptop, native reading, confirmed write/restore and
interrupt subscription/cancellation passed. The complete readable user-memory
image matched its original SHA-256 after restoration. These checks do not prove
that a physical phone tap was observed or that Linux hardware was accepted.

Protocol authority: [NXP NTP53x2 data sheet](https://www.nxp.com/docs/en/data-sheet/NTP53x2.pdf)
and [NTAG5 I2C interface application note](https://www.nxp.com/docs/en/application-note/AN12364.pdf).
The private Windows IOCTL ABI was recovered from the installed, hashed driver;
Microsoft's sample SPB IOCTL numbering is not interchangeable with it.
