# Qt UI language and appearance contract

Mandatory for agents changing UI, translations, appearance, UI preferences or their
packaging. The application implements live language/theme preferences in Manager, tray
and OSD; native desktop acceptance remains platform-specific. Use Qt 6.10 APIs compatible with pinned Qt 6.10.2.
The Qt 6.10 documentation currently identifies some pages as 6.10.3; do not infer
permission to upgrade the toolchain or use APIs introduced after our pinned version.

## User-visible behavior

- Supported UI languages: English and Russian. Source language is English.
  Language control: System / English / Русский; language names are autonyms, not flags.
  Default is System: choose the first supported language from QLocale UI preferences,
  otherwise English. Explicit en/ru overrides system language.
- Appearance control: System / Light / Dark. Default is System, including later OS
  changes while running. Persist the selected mode, not the currently resolved color.
- Place both labelled controls in Settings / Appearance, keyboard accessible and
  discoverable without knowing the current language. Apply immediately without restart,
  lost edits or recreating the settings window. A saved choice survives reopening/update.
- Apply preferences to settings, tray, OSD and future UI processes for the same user.
  Closing settings/tray must not disable background localization or OSD appearance.
  Existing OS-owned notifications/dialogs may retain OS-controlled language/appearance;
  do not promise arbitrary control of system UI.

System-language choice is resolved at launch and when returning to System. Handle
LocaleChange/re-resolve where Qt/platform delivers it; do not promise every desktop
broadcasts live UI-language changes. Explicit switching en/ru always works live.

## Ownership and placement

| Location | Owns |
| --- | --- |
| application/ui_preferences/{api,use_cases,ports} | Plain C++ preference values, validation and store/change-feed contracts |
| adapters/storage/ui_preferences | Revisioned per-user persistence; atomic updates of owned keys, safe migrations/concurrent writes |
| presentation/common/localization | Per-process LanguageController, QTranslator ownership and retranslation |
| presentation/common/appearance | Per-process ThemeController, Qt style hints and shared palette policy |
| presentation/i18n | Tracked application .ts catalogs, not generated .qm binaries |
| presentation/qml/pages, presentation/{tray,osd} | Manager pages (including Settings), tray/dashboard and OSD consuming shared controllers |
| apps/<graphical role> | Creates/injects controllers; restores preferences before creating UI |

ui_preferences is a supporting application module, not a new rich domain/aggregate.
The primary application is Redmibook Manager; Settings is one section, not its name.
Controllers are composition-owned Qt adapters, not mutable global Settings singletons.
Expose registered Qt/QML types with observable properties; inject them explicitly.
Preference IDs are stable values: language system/en/ru and appearance system/light/dark.
Keep these separate from translated labels and effective language/color scheme.
The indicator position is `osdPosition`: top-left/top-center/top-right,
center-left/center/center-right, bottom-left/bottom-center/bottom-right; default top-left.
`trayAutostart` is a boolean, default true. This per-user preference controls only
whether sign-in composition opens the optional tray, never service/session lifetime.
Settings provides a Test indicator action through the existing diagnostic use case.
`osdEnabled` and `osdSound` are independent daemon-owned preferences, both defaulting
to true. Hiding OSD does not suppress event sound. Sound has its own settings block;
turning OSD off also hides an already visible indicator. All feedback uses the same
presenter and event deduplication. Overview uses the shared microphone control port
and cached battery telemetry for charge/discharge power and estimated wear.

[Daemon-owned preferences](preference-authority.md) governs persistence and propagation.
The resident daemon owns per-user data, validates commands and caches the current
revision in RAM. Manager and tray use IPC snapshots and commands, never preference
file reads/writes, file watchers or preference-file locks. Deferred atomic writes
run on a daemon worker. Offline, incompatible and failed requests remain explicit;
there is no client-side persistence fallback. Isolated previews/test stores are separate.
Already-running clients must retranslate through en -> ru -> en via daemon broadcasts;
checking only a writer's translator does not establish synchronization.

Localization controllers still work if no tray/settings client exists. Headless
machine code returns stable diagnostic codes/typed values, never pretranslated UI text.

## Required translation mechanism

Use Qt Linguist: C++ QObject::tr()/QCoreApplication::translate and QML qsTr() with
literal source strings. Use translator comments/disambiguation when needed. No
handwritten RU/EN maps, JSON translation engine, language ternaries or wrapper that
hides source strings from lupdate. Format placeholders/plurals through Qt and keep
locale-sensitive number/date formatting separate from translated labels. Do not
concatenate translated sentence fragments. Preserve stable config/protocol/action IDs.
[Qt source translation guide](https://doc.qt.io/qt-6.10/i18n-source-translation.html).

Use QLocale::system().uiLanguages() for system selection, not country-only guessing.
UI language and regional formatting are different preferences: do not silently replace
the user's regional formats with a hard-coded country when selecting en/ru. Presentation
uses QLocale/Qt.locale formatting; wire/config numeric values stay locale independent.
[QLocale](https://doc.qt.io/qt-6.10/qlocale.html).

LanguageController owns translator objects for their full installed lifetime. Load
and validate the candidate catalog before replacing the active translator; on failure,
retain the previous working language and report failure. Use installTranslator/
removeTranslator on the GUI thread, then retranslate every live QML engine. There
is one translator lifecycle owner per process; do not mix it with a second implicit
QQmlApplicationEngine catalog loader. Store resources under an explicit project prefix
such as :/translations/manager_ru.qm, outside automatic adjacent qml_*.qm discovery.
[QTranslator](https://doc.qt.io/qt-6.10/qtranslator.html),
[QQmlEngine::retranslate](https://doc.qt.io/qt-6.10/qqmlengine.html#retranslate).

Do not assume engine retranslation updates cached C++ QString properties or QAction
text. Observe QEvent::LanguageChange: Widgets retranslate via changeEvent; plain
QObject presentation models use the documented application-event-filter approach
and emit relevant property/model notifications. Reformat stored typed diagnostics
from codes/parameters. Do not parse an English error message to recover meaning.
[Qt dynamic language changes](https://doc.qt.io/qt-6.10/i18n-source-translation.html#prepare-for-dynamic-language-changes).

## Catalog build and deployment

Require LinguistTools and qt_add_translations with explicit UI source targets, tracked
TS paths and reproducible resource prefixes. lupdate refreshes catalogs intentionally;
lrelease compiles catalogs during builds. Updating TS must not be an unexpected build
side effect. Russian TS is complete; English source-language plural forms also need
the normal Qt plural catalog workflow. Missing singular English translations fall
back to source text. Never construct translated identifiers at runtime for extraction.
[qt_add_translations](https://doc.qt.io/qt-6.10/qtlinguist-cmake-qt-add-translations.html).

Package generated QM resources with every graphical host needing them, including
resident OSD. Include the needed Qt-provided Russian catalogs through the documented
MERGE_QT_TRANSLATIONS mechanism; English Qt source strings remain the baseline. Resolve
the actual module catalogs at integration rather than inventing filenames or suppressing
missing-catalog warnings. The current NO_TRANSLATIONS deployment option must be reconciled
with that resource strategy before declaring translated Qt controls packaged. A clean
machine must not need Qt Linguist, development paths or network catalog downloads.

## Required theme mechanism

ThemeController is the only appearance policy owner in each graphical process. Use
QGuiApplication::styleHints(): Light/Dark maps to setColorScheme(Qt::ColorScheme::Light/
Dark), System maps to unsetColorScheme(). Observe colorSchemeChanged; never poll a
Windows registry key, theme file, timer or custom D-Bus endpoint instead of Qt.
Unknown means unknown: preserve the effective Qt palette rather than assuming Dark.
[QStyleHints](https://doc.qt.io/qt-6.10/qstylehints.html#colorScheme-prop).

Qt documents this API as a platform hint with backend limitations. The new palette
may not yet be installed inside colorSchemeChanged; custom presentation reacts to
actual palette changes. Any necessary unsupported-backend fallback must be centralized
in this adapter, based on a documented/tested Qt palette/style path. Never claim that
calling setColorScheme proves all surfaces changed, or add per-screen light/dark hacks.

Manager uses Qt Quick Controls FluentWinUI3 as its only production interface style.
Select it centrally through QQuickStyle::setStyle before loading any QML controls.
Use unqualified QtQuick.Controls imports; never switch style modules inside pages.
Legacy `design` values remain readable for compatibility; Classic does not select
Fusion in production. Settings has no style selector or Apply/restart action.
`theme` remains system/light/dark and applies live. Qt Widgets (tray and OSD) use
Fusion with the same shared palette. They do not instantiate Qt Quick styles.
Keep styling in this composition boundary and standard Qt palettes, without per-page
color engines or a hand-built imitation of Windows controls.
[Fluent WinUI 3](https://doc.qt.io/qt-6.10/qtquickcontrols-fluentwinui3.html),
[style selection](https://doc.qt.io/qt-6.10/qtquickcontrols-styles.html).

Standard controls inherit palette roles. QML custom drawing uses inherited palette
roles/shared semantic colors; Widgets use QPalette and respect active/inactive/disabled
groups. Status/accent tokens belong to one appearance module. No independent theme
tables in forms, OSD or menus; no literal black/white foreground/background patches.
The central light palette uses a soft gray window, white surfaces, dark primary text
and readable secondary text. Shared Surface outlines separate cards without page-local
theme logic. ChoiceBox retains native ComboBox behavior and popup while sharing a
palette-based outline and chevron, including on white cards. Isolated preset captures
support `tools/render-previews.ps1 -PresetSettings -Style FluentWinUI3 -Theme Light`.
Do not leave manual overrides that block returning to System. Icons must remain
legible against the effective palette. Respect accessibility/high-contrast information
reported by Qt; palette preferences never change hardware or other users' settings.
[Control.palette](https://doc.qt.io/qt-6.10/qml-qtquick-controls-control.html#palette-prop),
[QPalette](https://doc.qt.io/qt-6.10/qpalette.html).

## Acceptance for the implementation

- Switch en -> ru -> en repeatedly: existing/new windows, menus, OSD, errors, tooltips
  and accessible names update; plural/placeholder cases stay correct and edits survive.
- Test missing/corrupt catalog handling, unsupported system language, persisted startup
  and preference propagation to an already-running OSD process with clients closed.
- Check TS completeness, placeholders and plural entries with Qt Linguist tooling
  and focused validation; tests must not require generating fake translations at runtime.
- Exercise System/Light/Dark and live OS changes in System; manual selection remains
  manual until reset. Check open popups, disabled/new controls, custom surfaces/icons,
  high contrast and both languages without clipping Russian labels.
- Verify installed/portable resources with development paths removed. Offscreen checks
  do not prove native system theme following or compositor behavior. Windows acceptance
  comes first; Linux compile paths remain, with native Linux acceptance explicitly deferred.

## Reference provenance

Qt Visualization was inspected read-only: AGENTS.md section on Qt/theme/localization,
presentation/qt/src/theme/ui_text.cpp and renderer.cmake. It has centralized Theme/
Language, QTranslator and resource-packaged TS/QM catalogs, using centrally selected Qt styles and QStyleHints.
Adopt the centralized policy and palette-role discipline. Its Widgets ui::text/ui::bind,
static service access, QSS/docking/OpenGL details are not the QML architecture here and
must not be copied wholesale. Official Qt links above govern APIs; selection controls,
file placement, persistence and cross-process synchronization are project decisions.

## Monitor selection and translated update state

`osdScreen` stores a stable display identifier or `auto` (default). Auto means the
screen under the pointer when feedback occurs, with primary-screen fallback.
The placement control is one schematic display with nine position dots. It selects
the position for the active display, not an explicit multi-monitor topology. Choosing
a dot or testing the indicator resets a legacy fixed-display selection to auto.
Runtime placement still handles all screens and negative virtual desktop origins;
a disconnected legacy display falls back to auto. Never force all OSD to primary.
Opening Manager transfers activation coordinates to its existing process, moves
its own window onto that display and requests foreground activation through the
platform adapter. The OS still owns foreground permission.

C++ translated update status observes Preferences::languageChanged and emits its
NOTIFY signal; QQmlEngine::retranslate alone cannot invalidate a C++ property.
Visible update notices retranslate through the same preference notification.

## Shared surfaces and palette propagation

`roundedCorners` is a boolean, default true. Use the shared RoundedFrame for QWidget
tray/OSD/update notices, and Surface for application QML cards. The common radius
is 8 logical pixels; zero disables rounding. The indicator preview and license
viewer consume the same preference. Main-window corners use the platform adapter
(DWM preference on Windows, compositor policy on Linux). DWM is a hint, not proof
of visible rounding; QWidget transparent corners are rendered and tested directly.
Do not add per-window hard-coded color tables or duplicate rounded painters.

ThemeController synchronizes QGuiApplication and QApplication palettes. QML uses
the public Qt Palette/SystemPalette types in AppPalette, including active, inactive
and disabled groups. Observe QStyleHints::colorSchemeChanged and apply after Qt has
updated its platform palette. Check rendered window, selector and popup colors;
checking only QApplication::palette does not prove the QML scene changed.

Fluent image assets require native color-scheme hints. The offscreen backend reports
Unknown; headless smoke uses Fusion. Native Windows acceptance must separately
exercise Fluent and its open popups, plus System-mode reactions through Qt's backend.
Never report process-local Qt theme probes as changing the user's Windows theme.
Reserve space outside the content column for ScrollView's vertical scrollbar.

About provides a local full-text license viewer: canonical MIT, a clearly marked
unofficial Russian translation, Qt LGPLv3 and third-party notices. Keep English
original available. Translate via Qt Linguist; do not substitute runtime dictionaries.

## Neutral product palette and compact actions

The shared ThemeController owns neutral grayscale surface, accent and selection
roles for every appearance mode, including System. Follow the system light/dark
scheme, but do not inherit its colored accent or AlternateBase for product cards.
Pages consume shared palette roles; do not patch individual pages with color tables.
High-contrast accessibility is an exception: preserve the system palette and react
to QAccessibilityHints::contrastPreferenceChanged.

Place contextual actions on their section heading row, aligned right. The interface
design Apply/restart action sits beside the design selectors, appears only while
a pending design differs from the running design, and retains a fixed layout slot
when hidden so toggling cannot move the surrounding controls. Indicator testing belongs on the indicator heading row; runtime
health belongs in overview/diagnostics and tray status, not in that settings card.
About aligns developer and repository on one row, has a separate license card,
and displays the actual payload build identity beside the product version.
Tray context-menu labels stay short; its rounded stylesheet follows the common
rounding preference and Qt palette, and connected status has a check mark.

Transient Widgets surfaces use the shared appearance/widget_controls policy for
push buttons, segmented tool buttons and popup menus. Their visual states use Qt
palette roles and the common rounding preference. Do not assign per-button styles
inside tray/dashboard or leave one action on a different default Fusion look.
RoundedFrame composes the shared surface control policy; views own layout/actions.

Tray tooltips contain only product name and version. The mini-dashboard header is
one vertically centered row: product, version, microphone and runtime-health icons.
Omit the publisher credit here. Runtime health uses a compact standard Qt status
symbol (plug / disconnected plug / compatibility warning); its translated tooltip and accessible name distinguish unavailable session,
unavailable core, incompatible components and ready. Keep connection diagnostics
in About and the context menu. Header icons scale with the title font metrics and
device pixel ratio; microphone OSD scales its icon with its own text font metrics.

Do not insert pending/success labels into page layouts. Errors use the application's
message dialog; progress indicators keep a fixed slot beside the relevant action.
Refresh reuses the resident session's state feed instead of starting duplicate client
hardware reads. Reapply the shared palette only for actual theme changes, and preserve
selected-state contrast in the disabled palette group. Built-in display rate buttons
are shared by Overview and Devices. Indicator position and legacy display reset are
saved atomically. `osdSound` controls the standard system beep for device-mode OSD.
All OSD kinds use the single session-owned `OsdPresenter`: diagnostic test, Mi,
microphone, performance and refresh rate share one emphasized RoundedFrame, icon/text
layout, palette-derived border, rounding, placement, timeout and sound policy.
The diagnostic Test action must exercise that same renderer and sound preference.
Do not introduce separate diagnostic/vendor/microphone OSD windows. Native-key
handlers receive this presenter by reference; closing Manager/tray does not own it.
Confirmed GUI results use the version/build-checked, per-user feedback transport;
it accepts semantic state values only and cannot execute hardware commands. Queued
delivery is not proof of visible rendering, and failed delivery is not replayed.
Duplicate consecutive values per device kind are suppressed across OEM/GUI sources.
All status/device glyphs are monochrome SVG symbols tinted with the inherited
WindowText palette via `symbol_pixmap`; microphone mute uses a crossed microphone,
not green/red color. Every OSD kind includes a glyph; failures use a warning glyph.
Manager and tray read and change preferences only through daemon IPC. The daemon
owns the RAM revision and deferred atomic QSaveFile persistence on its worker;
clients do not read/watch settings files or take preference-file locks. A per-user
authority lifetime lease prevents duplicate owners and is released only after the
store flushes, as specified in preference-authority.md.

The application mark is presentation/assets/app-icon.svg. app-icon.ico is its
multi-resolution Windows derivative (16 through 256 px). GUI resources and Windows
EXE/setup resources use that same mark; Linux installs the SVG in the hicolor theme.
The tray reuses the application icon instead of painting a separate letter/font.

Battery-page auto-refresh renews a session-owned sampling lease every second only while
the page is visible and its window is active and not minimized. Navigation/hiding releases
it; disconnection or missing renewals expires it after 3.5 seconds (500 ms watchdog tick).
The selector offers 100 ms, 1 sec (default) and 5 sec in both languages,
persisted as batteryIntervalMs (100, 1000, 5000). Legacy 5 ms maps to 5000 ms.
The session samples at the minimum interval of live clients; only one native job can run.
These are requested sampling intervals, not guarantees of new driver measurements or
real-time scheduling. Do not repeat full firmware/audio/display refresh on these ticks.
Native failure pauses fast sampling; power events, manual refresh and coarse background
observation can recover it. Stable telemetry emits no new state frame. Changed battery
values use a separate notification; unrelated controls are not invalidated. Qt text
properties handle equal rendered values without forcing repaint/update of the window.
Background policy protection keeps its independent machine lifetime and ten-second
native telemetry guard; it does not depend on a visible UI or a session sampling lease.
Show Charge / Discharge and absolute W; at zero use native discharging state, otherwise
Charge. Wh remains the capacity unit. Missing rate remains unknown, never zero.

Tray configuration uses stable section IDs from UiPreferences, stored in traySections.
The expanded Tray settings section exposes the same list; the dashboard registers native widget sections against
these IDs. Visibility is configurable for performance, refresh, limit, power, microphone
and connection. Adding a section requires its ID, translated label and widget mount;
it must not introduce a duplicate hardware/policy owner. Connection uses a chain-link
glyph, distinct from battery/power. Header limit and directional W use shared state.

Tray configuration is expanded within Settings, with its own heading and shared scroll area.
The dashboard shows power, charge limit, microphone and connection in that order.
Hide rates that round to 0.0 W. An open dashboard renews the shared battery sampling
lease every second at 1000 ms and requests a fresh sample immediately. Closing it releases
the lease; the session retains a ten-second background cache refresh. Manager and tray
share the same sampling authority; neither polls firmware for battery telemetry.

The tray header is one row: brand/version, optional nonzero power, charge-policy
symbol, microphone, connection. Do not add a second indicator row or visible limit
text. The battery mark with an arrow ending at a stop bar denotes charge policy; the actual threshold is in its
localized hover tooltip and accessible name, never an unlabeled state-of-charge
percentage. Icons stay within their SVG viewBox including stroke bounds.

Performance selectors use the visual order Quiet / Balanced / Performance
(Тихий / Сбалансированный / Производительный). Keep firmware profile IDs separate
from this visual order; renaming Turbo in presentation does not change its native value.

For shared Surface, keep leftPadding/rightPadding/topPadding/bottomPadding bound to
padding explicitly. Fluent control definitions may otherwise supply individual edge
bindings that defeat a caller's uniform padding value. Keep selected navigation and
icon foregrounds tied to HighlightedText/ButtonText roles; neutral accents must not
produce white-on-white selected content in isolated previews.

The FluentWinUI3 documentation supports using the style across platforms; do not
blanket-disable it for previews. The public preview-style override is restricted to
isolated offscreen capture. A focused -PresetSettings -Style FluentWinUI3 capture
is not a replacement for full theme/language or native compositor acceptance.

Settings provides a confirmed reset of user preferences, including shortcuts and custom
tray presets. PreferencePort resets the owned ui object atomically under the normal
daemon authority, increments the revision and preserves other record fields and the
configured display refresh choices. All clients receive the usual preference
notifications. Language/theme and other live preferences update immediately;
the interface remains Fluent. Reset never changes the
physical display mode. Before resetting user preferences, the device controller
asynchronously restores the 100% charge policy on qualified hardware through the
existing core port and verifies firmware readback. A failed, unavailable or unknown
result blocks the preference reset. Unimplemented or unqualified charge control is
skipped without native writes; the confirmation states that battery restoration applies
on supported hardware. Firmware readback confirms policy, not physical cutoff. Empty
refresh choices mean the advertised subset of standard 60/120/165 Hz choices, not a
default physical refresh rate. Never implement it as a sequence of per-key writes or
delete the whole user-data directory.
