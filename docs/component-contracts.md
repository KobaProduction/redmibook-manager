# Background component interfaces

Interface-first design for the next iteration, not an implemented API. Concrete
standard-C++ declarations and focused contract tests precede each platform adapter.
Do not create unused placeholder frameworks. No Qt, Win32, systemd, D-Bus or pipe
types in domain/application interfaces.

Ownership and physical placement follow [module layout](module-layout.md). The broad
names below describe interaction shapes, not a universal base API: split DeviceControl
into battery/performance consumer-owned ports and StateEventSource into the observations
needed by each use case. Feedback receives its own normalized model via integration
adapters. No shared hardware facade or all-purpose event-bus dependency is implied.

| Port | Contract operations | Responsibility |
| --- | --- | --- |
| DeviceControl | capabilities; readState; requestChange -> result | Typed policy/mode operations; accepted request differs from confirmed effect |
| InputEventSource | capabilities; bind actions; subscribe; cancel | Normalized activations/device-key events, explicit binding conflicts/denial |
| StateEventSource | snapshot; subscribe from sequence; cancel | Authoritative state, including external changes; explicit resync if history unavailable |
| FeedbackPresenter | capabilities; present; dismiss | Session OSD and delivery status; no system command execution |
| NotificationPresenter | capabilities; notify | Desktop notifications, separate from custom transient OSD |
| UserActionExecutor | execute configured action -> result | User-session execution, never in privileged machine runtime |

Ports belong to their use cases, not a service locator. In-process and IPC clients
preserve application-facing semantics; wire DTOs are separate from domain objects.
Lifecycle/update host operations belong to installer/host adapters, not hardware domain.

## Data and operation semantics

Microphone firmware commands use the authenticated machine allowlist and return
the observed firmware flag separately from the actual audio endpoint state. Only
TM2309/P0A0A is qualified for flag-to-LED control. Manager commands never write the
Windows audio endpoint directly. The resident session reads firmware intent after
announcements and applies it asynchronously, with ten-second reconciliation for
missed events. Events and reconciliation never issue a firmware SET. Readback
mismatch, unknown write outcome and unavailable endpoint remain distinct failures.

Battery telemetry is a separate read-only application port and an optional field in
the version/build-checked device-state projection. Missing driver values remain unknown;
policy readback, battery charging state and physical cutoff are separate facts.
Machine-owned MaintainChargePolicy serializes explicit selections and reconciliation
through ChargeControl and PolicyStore. Intent is persisted before an authorized write;
failed storage/read/qualification forbids automatic SET. Reconciliation writes only on
known drift. The machine's ten-second guard also accepts fresh native battery telemetry:
AC online, charging, positive known W and charge at or above 80% permit a journaled 100 -> 80
rearm only when persisted intent is 80 and firmware still reports 80. A pending marker
spans the whole sequence; no intermediate 100% intent is persisted. No further SET is
issued after an unconfirmed phase. Both matching readbacks confirm policy, not physical
cutoff; later telemetry must show whether charging stopped. A flushed pending marker prevents replay after an uncertain write/restart.
UI/session lifetime does not own retained policy or automatic reconciliation.
The session state pipe also accepts a battery-only read request. It emits no full
refresh/progress event and cannot invoke firmware commands; only telemetry observation
is scheduled and coalesced. battery-watch accepts only 0 (release), 100, 1000 and 5000 ms;
each socket owns a 3.5-second lease. Renewals do not perform native reads themselves.
The minimum live interval owns the session sampler; stale/disconnected leases stop it.
Connection establishment may perform the
existing initial full refresh once. Page auto-refresh does not repeat that full request.

The resident read-only device-state channel acknowledges explicit refresh requests
with their request token after both local-device and firmware read jobs finish.
Cached/intermediate snapshots and acknowledgements for other tokens do not complete
the requesting client's wait. A bounded deadline and disconnect produce typed
TimedOut/Unavailable outcomes; timeout never implies cancellation of native calls.
Refresh progress is separate from command execution, so status reads do not disable
device controls. GUI operation errors use explicit presentation error signals and
closeable dialogs, rather than layout-changing success/pending/error rows.

- Capabilities distinguish supported, unsupported, permission required/denied,
  backend/session unavailable and failed probe. Unsupported is not a successful no-op.
- Intents contain stable action IDs and typed parameters with verified execution scope;
  privileged APIs cannot be an arbitrary string-command interface.
- Observed state includes known/unknown/unsupported and source. Confirmed firmware
  effect requires authoritative readback or documented effective-state observation.
- Events carry event ID, source-local sequence, affected scope, observed state and
  optional operation correlation ID. Endpoint authentication determines trusted scope;
  message fields never grant authority. Correlate duplicate observations.
- Feedback carries semantic kind, requested/observed/failed status, optional typed
  value, bounded duration/coalescing key and target session. Presentation localizes
  text. Results distinguish rendered, handed to OS backend, suppressed and unsupported/
  failed; dispatch alone does not prove visible display.
- Async operations have operation IDs, bounded deadline/cancellation and explicit
  results. Before threaded adapters, specify callback/executor ownership, ordering,
  bounded queues and cancellation completion in concrete C++ contracts.

Subscription lifetime belongs to the background use case. Cancelling a tray/window
subscription cannot cancel background subscriptions. Snapshot/subscribe ordering
must avoid lost-event races, with explicit resync. Reconnect must not replay stale
popups; coalesce repeats and keep user/session scopes isolated.

## Feedback policy and lifetime

`application/feedback/device_feedback.hpp` is the implemented plain-C++ contract
for confirmed microphone/performance/display feedback. Its presentation port
distinguishes queued, presented, suppressed, unavailable and invalid delivery.
The per-user session IPC accepts only bounded, matching version/build feedback
frames and routes them to the same presenter as physical keys and diagnostic tests.
It cannot issue device writes. GUI deadlines/unconfirmed results must not publish
confirmed feedback; disconnected feedback is dropped without retry/replay.

Session application logic maps relevant state/result events through presenter ports.
Tray/settings are optional subscribers/command clients, not owners of input or OSD.
Keep a session runtime/OS presentation backend alive after tray closes. A helper
renderer must be supervised independently, without reopening unwanted client UI.

A key press/accepted command cannot emit confirmed success. Pending OSD must say
pending. Caps/Fn-lock or keyboard mode may already be changed by firmware/OS: observe
and render without toggling twice. If no authoritative observation exists, report
that capability honestly. Avoid duplicating desktop-provided OSD where configurable.

## Contract verification

Use in-memory source/device/presenter fakes to prove tray/settings closure leaves
background routing active; external state events do not execute commands; denied/
unsupported operations cannot emit confirmed success; duplicates/stale/repeated events
do not flood popups; cancellation/reconnect and multi-session routing are correct.

Each implemented port change includes Windows/Linux compilation paths. An unimplemented
Linux adapter may explicitly report unavailable; compilation is not runtime acceptance.
Windows-first testing never justifies OS/Qt types in shared contracts.

Session status responses append the resident session version/build after the daemon
identity. The shared adapter codec accepts the previous seven-field reply for
recovery/diagnostics, but missing session identity is incompatible, never Ready.
Ready requires the UI, session and daemon to share version and payload build ID.

## Native request execution

Manager microphone/display operations and session hotkey actions run native ports
on workers and deliver completion on the Qt GUI thread. No operation captures a
presentation object into a worker: shared ownership of the injected ports keeps
dependencies alive if a window closes. Each controller admits one request at a time.
Controls show pending/busy state and do not dispatch repeated overlapping changes.

The presentation deadline is ten seconds. On expiry, discard stale observations and
do not turn a late completion into a success popup. A synchronous OS call can still
be running after that deadline: do not imply cancellation or free its dependencies.
The WMI adapter uses semisynchronous completion with one bounded result wait. Native
completion and authoritative readback replace Sleep/fixed-delay retries. Readback
confirms the reported setting, never an unobserved physical effect.

The developer launcher awaits named-pipe availability and process-completion events
with cancellation deadlines. It queries the session executable for its endpoint;
PowerShell does not duplicate the session identity/hash algorithm. CLI entry points
may wait at their console boundary while GUI event loops remain responsive.

VendorInputPort delivers typed firmware intentions/announcements from a bounded
worker queue; Ready means subscription accepted, not physical-key acceptance.
Stop invalidates callback reservations immediately and exposes cleanup completion
through shared_future. An already-reserved callback can finish, so session composition
also guards its QObject delivery lifetime and bounds queued GUI delivery. Overflow
never replays an uncertain Mi sequence. Microphone intent coalesces to the latest
explicit state while a previous native call finishes; it is not a second toggle.
Startup profile discovery also runs on a worker. No firmware-state guess may unmute
capture at startup. Driver-advertised hardware mute is separate from actual mute,
API readback and physical recording acceptance.

## Qualified firmware and device-state channels

The machine host accepts only the charge-capability/read/80/100 and performance-
capability/read/Balanced/Quiet/Turbo commands in `contracts/local_api/firmware_protocol.hpp`.
It verifies the client's active console session and exact version/build before dispatch.
The client authenticates the server PID against SCM. Identity comes from SMBIOS in
the host; callers cannot provide raw firmware packets, BIOS overrides or algorithms.
Commands execute serially; unknown native write outcomes latch subsequent writes off.

The resident session publishes `application/overview/DeviceState` over a separate
per-user local channel. Its only incoming request is a read resynchronization.
Frames require schema/version/build compatibility and are bounded to 64 KiB;
slow subscribers disconnect. Manager and tray subscribe independently. Audio endpoint
callbacks, display events and completed operations request fresh observations.
Local audio/display and machine firmware reads have independent worker lifetimes and
deadlines so a slow firmware provider does not delay microphone feedback.

### Configurable session hotkeys

Manager sidebar order is Overview, Device, Hotkeys, Settings, About. Device owns
battery charge control/telemetry, display, brightness and audio controls in one
scrolling view, without tabs or nested battery/control pages. Tray customization
is a subpage of Settings, with an explicit return action. Moving the battery view
does not change its visibility-scoped telemetry lease.

Brightness and energy saver use separate plain-C++ ports, with Windows/Linux
factories selected in CMake. PowerControlsModel executes native operations on a
worker and retains its occupied slot until actual completion, including after a
UI deadline. Visible display/audio controls refresh every two seconds; hidden or
inactive views stop requesting observations. Linux leaves these modules unassembled.
Windows brightness targets the unique active WMI brightness instance and confirms
the result by reading CurrentBrightness. Energy-saver observation is separate from
control capability. The UI consumes the adapter-derived module catalog and renders
no controls for unassembled or unqualified features. A previously qualified control
can still show a real operation failure; failure is distinct from feature absence.
Windows manual energy-saver support is qualified through its native override route
and observed energy-saver notifications. Manager and preset execution share the
same adapter instance in their composition. Requests do not alter automatic
thresholds or administrative policies and never emulate a click in Windows UI.

Hotkeys (Хоткеи in Russian) is a top-level Manager sidebar section, separate from application Settings.
It owns the vendor-key and Fn+S bindings. The proprietary
key uses the approved MingCute opposed-toggle symbol rather than the text “Mi”.
`vendorKeyAction` defaults to opening Manager Overview; it can also open the tray
mini-dashboard or do nothing. Existing persisted Fn bindings retain their numeric
values (0 default, 1 Overview, 2 none); 3 opens the tray dashboard. Resetting UI
preferences restores defaults. The session refreshes preferences before dispatch,
so no runtime restart is required. Fn+K is a fixed, read-only entry: switch the
firmware performance mode. Its former `fnKAction` preference is ignored on read;
new writes to that key are rejected. Firmware profile notifications refresh state
and OSD only: event 0x16 also occurs after software profile SET and must never be
treated as a distinct rebindable key press.

Fixed shortcut actions use the same native ComboBox as editable bindings, disabled
inside an enabled hover target. Explain firmware restrictions in a hover tooltip
and the accessible description, without a permanent warning row or custom styling.

## Firmware microphone intent and endpoint mute

`microphone::FirmwareStatePort` provides qualified GET and explicit SET/GET control,
separate from `microphone::ControlPort` (observed Windows endpoint mute). The machine
service authenticates version/build and console identity. GET remains allowed after
an unknown mutation but cannot clear the write latch; subsequent SET stays blocked.
Only TM2309/P0A0A is qualified for the observed firmware-flag/LED relationship.
Linux composition does not supply this vendor feature. Missing values remain unknown.

Subscribe first, then query asynchronously after input readiness. Known qualified
startup intent reconciles both mute and unmute per the owner's LED-authority policy;
unknown/denied reads never authorize an audio change. A newer native notification,
overflow or source failure invalidates the initial GET. Ordinary synchronization
also captures native-ingress generation and checks it after endpoint read, before
any SET; superseded work cannot deliberately apply the observed stale intent and
its result is discarded. Native calls do not hold the delivery mutex. Physical key
changes and Windows audio writes are not an atomic cross-device transaction.

Events invalidate intent and trigger a fresh GET; they never echo firmware SET.
The existing ten-second reconciliation recovers missed events. Endpoint writes only
occur on observed mismatch and require exact audio readback. Manager's positive
microphone switch projects firmware intent; status icons/OSD project actual audio.
The two observations remain distinct on failure. The driver hardware-mute capability
is not independent physical LED or audio-privacy proof.
