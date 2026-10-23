# Tray presets

Presets are per-user configuration. Production defaults include an editable Power
preset with Economy/Performance buttons, assembled by the daemon from qualified
capabilities and hidden in the tray until the user enables Show in tray.
Tray visibility controls and the preset builder are expanded directly in Settings
under Tray settings, within its shared scroll area. They are not a nested settings page.
Creating one adds exactly two buttons with no actions; at least two remain in each
preset. Users may delete a preset, rename it, hide it, reorder presets and buttons,
and add up to eight buttons. Each button has text, an icon and up to eight ordered
actions. The modal editor keeps a draft until Save; Cancel leaves stored data intact.
Long captions elide inside their buttons; hover and accessibility retain full text.

## Feature boundaries

- application/presets owns plain C++ configuration, validation and ordered execution.
- application/power owns the power scheme port; Windows and Linux implementations are
  separate adapters. Linux currently returns NotImplemented and contributes no choices.
- adapters/integration maps preset actions to the existing qualified performance port,
  the positively identified internal display and the power scheme port.
- adapters/storage/ui_preferences owns JSON serialization and atomic revisioned saves.
- presentation/presets and presentation/qml/presets own the editor feature and its
  reusable cards/action rows. presentation/tray/preset_panel owns native tray rendering.
- apps composes adapters and runs native queries/commands on workers.

This feature grouping provides FSD-style UI slices within the existing inward-facing
Clean Architecture dependency rule; it does not replace application/domain with UI code.
New OS/build restrictions enter through the injected capability catalog, not platform
branches in QML. Unsupported saved actions remain editable and are never silently
substituted with another action. Stable action kind IDs are 0 performance, 1 internal
screen refresh, 2 Windows power scheme, 3 energy saver, 4 internal-display brightness.
Values are typed by their selected catalog. Brightness offers 0–100%; energy-saver
on/off choices appear only when the injected adapter reports controllable support.
Windows qualifies manual energy-saver control from its native override payload
and the Windows 11 energy-saver notification. Qualified adapters contribute the
On/Off preset choices; they confirm the actual system transition before reporting
success. Automatic activation thresholds and administrative policies are unchanged.

## Power scenario contract

A button composes several independent actions; it is not restricted to a fixed
set of four settings. Retain every existing action when adding new capabilities:
firmware performance profile, internal-panel refresh rate, Windows power scheme,
manual Windows energy saver on/off and internal-panel brightness. Further actions
extend this catalog through qualified application ports and platform adapters.
Automatic energy-saver thresholds and always-on administrative policies are outside
this feature and must not be changed by a manual preset.

The owner's intended scenarios are an economical profile (for example 60 Hz,
Quiet firmware profile, a selected Windows plan, manual energy saver enabled and
reduced brightness), a performance profile (highest selected supported refresh
rate, Performance firmware profile, another Windows plan, energy saver disabled
and chosen brightness), and an intermediate development profile. These are user
compositions, not forced defaults, fixed values or guarantees of battery runtime.
Saved presets, button order and
previously available actions must survive catalog extensions.

The daemon asynchronously discovers the qualified catalog, then creates the editable
Power default. A persisted initialization marker prevents recreation after deliberate
deletion. Reset restores the hidden default atomically from the cached catalog; existing refresh
choices remain intact. There is no Add example button. Initial migration preserves
existing custom presets and already edited default captions. Upgrading the previous
English example hides it once and marks only unchanged factory captions for translation.
Economy uses 60 Hz, energy saver on, Quiet and 50% brightness; Performance uses the highest
selected supported standard rate (165/120/60 Hz), energy saver off, Performance and
100% brightness. Unsupported actions are omitted. No Windows power scheme is added.
Creation never applies actions or changes refresh choices. Ordinary initialization
never replaces user presets; reset intentionally restores the default preset.
Factory captions retain typed default-title identities and use Qt translations in
Manager and tray, including live language changes. English is the source fallback.
Renaming a preset or changing a button caption clears its default identity; user text
is never translated. Saving unchanged actions preserves the identity of an unchanged
caption. Custom presets have no default identity, even if their text matches a factory name.

The execution sequence is deliberate: a later system setting can affect an
earlier one. In particular, enabling Windows energy saver can reduce brightness;
place an explicit brightness action afterward when that final brightness is wanted.
Unqualified OS builds omit the manual energy-saver action; a native request whose
result cannot be observed stops the sequence with an unconfirmed outcome.

## Execution behavior

Catalog discovery is read-only and asynchronous with a ten-second UI deadline. The
sequence is preflighted before any mutation. Actions execute in stored order, stopping
on the first unconfirmed result. Previous changes remain applied; there is no implicit
rollback. The result reports completed actions or preflight unavailability separately.
The UI keeps a single operation active until its worker really finishes. Once its
execution deadline expires, no subsequent action starts; an already-running native
call is not claimed to be cancelled. Firmware writes still use the authenticated,
version/build-checked core allowlist and its existing unknown-outcome protection.
Display writes target only the qualified internal panel and an advertised refresh rate.
Windows plans use PowerEnumerate/PowerReadFriendlyName/PowerSetActiveScheme and active
scheme readback; identifiers come from enumeration rather than hardcoded UI choices.

## Development preview

`tools/render-previews.ps1 -Build` renders the editor and tray using isolated settings
and a sample preset. `-TrayOnly` renders the native tray without building Manager.
`tools/render-previews.ps1 -InteractivePresets -Build` opens a separately named preview
window with the sample preset. Its actions affect simulated state only, never BIOS,
display hardware, microphone or Windows power settings. Production preferences and
installed processes stay untouched. Do not launch it over the owner's workspace
without a request; screenshots are the default review path.

Targeted acceptance: presets-contract checks persistence, minimum button count,
ordering, unsupported preflight and stop-on-failure execution. presets-ui-contract
clicks the actual native preview buttons offscreen and checks projected state.
Windows plan mutations are implemented but are not claimed as hardware-accepted by
these fake-port tests. Full verify and installation are separate owner-authorized steps.

The button editor keeps its text field and selected icon on one row. The icon
opens a compact five-column palette of 25 monochrome glyphs, with localized
tooltips/accessibility names. The application icon catalog validates persisted
choices and supplies stable IDs to the presentation palette;
never use a text-only icon-name ComboBox. Each action row aligns its kind, concise
value and ordering controls. Tray visibility settings use a responsive two-column
layout, with consistent control heights and inherited palettes. Review screenshots
must include the Tray configuration page, the action editor and the resulting tray.

## Fluent design review

Review the real Qt Quick style with:
`tools/render-previews.ps1 -Build -PresetSettings -Style FluentWinUI3`.
This uses the public FluentWinUI3 style with an isolated dark fixture and captures
only the Tray page and button editor. It is a design capture, not a replacement
for the unchanged full UI contract or native system-theme acceptance.
The default test backend still uses Fusion when no preview style is requested.

Preset cards present the horizontal button group before editing controls. Titles
are read-only summaries until Rename is chosen; secondary ordering/removal actions
live in accessible menus. The settings grid uses labelled cards with switches on
the right. New custom surfaces use shared Surface and inherited palette roles.
Spacing follows a small 8/12/16/24 logical-pixel scale and section headings express
hierarchy rather than oversized control boxes. These are layout decisions, not
an independent per-page theme or a new external widget framework.

References: https://doc.qt.io/qt-6.10/qtquickcontrols-fluentwinui3.html,
https://doc.qt.io/qt-6.10/qtquickcontrols-customize.html,
https://learn.microsoft.com/en-us/windows/apps/design/basics/content-basics.
