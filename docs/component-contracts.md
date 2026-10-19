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
