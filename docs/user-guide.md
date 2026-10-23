# User guide

[English](user-guide.md) · [Русский](user-guide.ru.md) · [README](../README.md)

## Getting started

Install the Windows x86_64 EXE from Releases and approve UAC. The installer explains
Manager, machine service, resident session and tray. It preserves preferences on
upgrade. Launch Manager from Start; it runs as a normal user. Firmware commands go
through the authenticated machine service. The ZIP alone does not install services.

The interface follows the system language, falling back to English, and supports
live English/Russian selection. System/Light/Dark appearance uses Fluent controls.
The startup window shows a centered spinner while content loads, then the main UI.
If background components cannot be reached, retry and diagnostics remain available;
controls do not silently save settings locally or claim hardware success.

## Navigation

1. **Overview:** concise device identity and assembled controls, including refresh,
   charge policy, battery charge/rate/wear and microphone where supported.
2. **Device:** battery telemetry, charge limit/manual energy saver, microphone,
   built-in display and BIOS/system-board information. Unknown fields are omitted.
3. **Hotkeys:** special vendor-key action, configurable application shortcuts and
   the fixed firmware-controlled Fn+K entry with a hover explanation.
4. **Settings:** appearance/language, startup, OSD, sound, expanded tray settings,
   presets and reset. The tray builder is not a separate nested window.
5. **About:** application/runtime identity, connectivity, licenses and updates.

## Battery and display

80% selects protection; 100% allows full charging. The displayed current percentage
is independent of the policy. Selecting 80% at 91% does not actively discharge the
battery to 80%. A ten-second machine guard detects charging at or above the limit;
readback and confirmed recovery are required. A healthy reading causes no write.
An unknown write outcome blocks further writes until reconciliation.

Power is an instantaneous rate in **W** (charge or discharge); energy/capacity is
**Wh**. Wear is an estimate from full-charge versus design capacity. Values depend
on sensor freshness. Battery details offer **100 ms / 1 sec / 5 sec** while visible;
leaving the view releases fast sampling. Open tray requests fresh telemetry every
second; hidden clients do not keep that subscription alive.

Only the identified internal display is controlled. Choose the supported rates
that should participate in cycling. Resetting application preferences preserves
the current screen mode; it restores the battery policy to full charge.

## Hotkeys and microphone

The special vendor key can open Overview, open the tray or do nothing, with other
choices supplied by the supported action catalog. Fn+S is a session action;
Fn+K changes firmware state itself and cannot be reassigned in Manager.

On the qualified firmware, microphone commands change the firmware intent flag,
confirm it, then reconcile the Windows communications endpoint. The physical key
uses the same observed intent, without echoing a second firmware toggle. Driver
and device remain enabled. The LED flag and Windows endpoint state are separate
observations; errors must not be presented as a confirmed synchronized mute.

## Presets and feedback

Tray settings control visible status elements and sections. A factory Power preset
is present but hidden by default. Enable Show in tray to display it. Economy uses
60 Hz, Quiet, energy saver on and 50% brightness where supported. Performance uses
the highest selected supported standard rate (165/120/60 Hz), Performance, energy
saver off and 100% brightness. No power scheme is forced by the example.

Create a custom preset, edit its buttons, choose icons/captions and add ordered
actions. Supported choices include firmware profile, refresh, Windows power scheme,
manual energy saver and brightness. A new custom preset starts with two empty buttons;
up to eight buttons and eight actions per button are supported. Reorder or hide
presets as needed. Custom names do not translate; unchanged factory names do.

Actions are preflighted and executed in order, stopping on an unconfirmed result.
Earlier successful changes remain applied: there is no automatic rollback. Place
brightness after energy saver if the final explicit brightness must win. Presets
never change Windows automatic energy-saver thresholds or administrative policies.

OSD visibility and event sound are independent. Test uses the shared feedback path
and respects these preferences. Closing Manager/tray does not stop resident feedback,
hotkeys or the machine guard. Disabling tray autostart affects the tray only.

See [validation](validation.md) for hardware and OS limitations.
