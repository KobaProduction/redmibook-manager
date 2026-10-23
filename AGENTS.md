# Redmibook Manager — agent entry point

Native C++20 / Qt 6.10.2 / QML application for Windows and Linux. No WebView.
The owner directs the product; agents implement, build and verify it.

**Current owner instruction:** do not spawn, resume or delegate to other agents.
Existing workers may finish their assigned work; the primary agent completes integration.
This instruction overrides the delegated Reviewer requirement while it remains active.

## Read only the relevant authorities

- Start with [architecture and navigation](docs/architecture.md).
- Module placement, public boundaries and composition follow [module layout](docs/module-layout.md).
  It describes future structure; add no empty skeletons or implementations during architecture-only work.
- UI/language/theme work must first read [Qt UI conventions](docs/qt-ui-conventions.md).
  Support English/Russian through Qt Linguist/QTranslator and live QML retranslation.
  Centralize System/Light/Dark through QStyleHints and inherited palettes. No custom
  translation dictionaries, per-screen theme tables or duplicated controller owners.
- Product/version decisions: [product contract](docs/product.md). Publication work also
  reads [release policy](docs/releasing.md). Current base version is 1.1.5.
  Version changes belong to owner-authorized checkpoint commits: at most one patch
  increment per checkpoint. Do not bump the version for uncommitted edits, builds,
  previews, packaging or installations. Rebuilds before the next checkpoint keep
  the same version; use payload build identity to distinguish their binaries.
  Major/minor changes require an explicit owner decision. CI never bumps versions.
- The shared [AI Agent Workflow entry map](https://github.com/ArthurKoba/ai-agent-workflow/blob/93d5e3eebd3b693c33930a3127e168e48cdd4143/AGENTS.md)
  governs task routing. Read selected task skills completely, not the entire library.
- Implementation: [Clean Architecture / DDD](https://github.com/ArthurKoba/ai-agent-workflow/blob/fdb39e72f330b5e6622895cc31ca8e7b73699629/skills/software-engineering/clean-architecture-ddd.md).
- Toolchain/hooks/CI: [Project Bootstrap](https://github.com/ArthurKoba/ai-agent-workflow/blob/93d5e3eebd3b693c33930a3127e168e48cdd4143/skills/project-bootstrap.md)
  and [local quality contract](docs/development.md).
- Git/publication: [Git Workflow](https://github.com/ArthurKoba/ai-agent-workflow/blob/93d5e3eebd3b693c33930a3127e168e48cdd4143/skills/git-workflow.md).
- Documentation: [Context Engineering](https://github.com/ArthurKoba/ai-agent-workflow/blob/93d5e3eebd3b693c33930a3127e168e48cdd4143/skills/context-engineering.md).

## Repository decisions

- **Owner gate: do not spawn or resume subagents.** Finish implementation directly.
  The owner must explicitly reauthorize delegation before this changes. Existing
  workers may finish already assigned work. This overrides the delegated-review
  requirement below while this gate is active.

- **Use the host's built-in code navigation and refactoring tools first.**
  The owner has explicitly disabled Serena for this Codex checkout. Do not call,
  reconnect, start or re-enable it without a new explicit owner instruction.
  In the current Codex environment, use its available built-in tools; do not start,
  initialize or use Serena when these tools already cover the required work.
  Check actual tool availability and capabilities rather than assuming that a product
  name or a plain text search provides semantic refactoring support.
  Serena is a fallback only when suitable built-in/connected code tools are absent.
  When that fallback is needed, read its `initial_instructions`, activate this checkout,
  and reuse **one Serena server instance and C++ language server per checkout**.
  Restore the existing connection before considering a launch; do not create duplicate
  instances for another turn, retry, reviewer or client. Identify project ownership
  before replacing a failed owned instance, wait until it has stopped, and never stop
  another project's server or bypass an automatic approval rejection. Record any
  manually managed instance's PID/project/endpoint locally for later reuse.
  If neither suitable built-in/connected navigation and refactoring tools nor a working
  Serena fallback is available, this is a blocker for affected code work and review.
  Explain which capabilities are missing and obtain explicit user permission before
  proceeding without them. Tool failure is not permission to substitute text edits
  for semantic refactoring. Ordinary file/shell tools remain appropriate for documents,
  configuration, new files, builds and operations outside semantic refactoring.
- Every authorized parallel review chat or delegated code task must receive this tool
  selection order and blocker rule. Each agent checks its own available capabilities;
  do not pass a blanket mandatory-Serena instruction to other agents.
- Domain/application are standard C++ only; inject OS services through application ports.
- Native queries/commands must not block the GUI thread. Use completion events/signals,
  bounded asynchronous requests and explicit pending/failure/timeout results. Where a
  native API is synchronous, isolate it on a worker with safe dependency lifetimes.
  Do not use Sleep, fixed-delay retries or GUI event pumping to wait for hardware.
  A UI deadline never proves that the underlying operation was cancelled.
- State propagation is event-driven and asynchronous. Subscribe to native/core changes,
  coalesce notifications and reconcile authoritative state with one bounded background
  task every 10 seconds to recover missed events. Publish only actual state changes.
  Do not add busy loops, sleep/retry loops, per-page polling or repeated reads on each UI
  refresh where a change notification exists. Background reads must not toggle command
  busy state, disable controls or cause UI flicker. Continuous polling needs a concrete
  hardware/protocol requirement; the retained charge-limit guard is an explicit exception.
- QML is presentation. Hardware access belongs to platform adapters, assembled in app entry points.
- The resident daemon is the sole authority for user preferences. It loads settings
  once, keeps the current revision in RAM, serializes and validates incoming commands,
  broadcasts accepted changes, and coalesces durable writes. Manager and tray use IPC
  snapshots/commands only: no production client may read/write/watch settings.json or
  coordinate preference-file locks. Failed/offline/incompatible commands are explicit;
  never fall back to client-side persistence. Machine firmware policies retain their
  privileged machine authority. Isolated file-store test/preview fixtures are separate.
- Composition and qualified adapters provide the available feature catalog for each
  OS/device/firmware combination. Render only assembled, supported features in UI and
  preset choices. Do not add OS-name branches, read-only substitutes for requested
  controls, or "not available on this platform" placeholder cards. Actual operation
  failures remain distinct from an absent feature and must not be disguised as success.
- Use recognizable product/component names in UI and installer helper filenames shown
  by Windows. Avoid opaque abbreviations such as `rmb-maintenance` in user-facing prompts.
- Keep unsupported, unimplemented, failed and confirmed hardware states distinct.
  Never infer successful charge control from a successful API call alone.
- Keep UI/core version and payload build compatibility explicit. Health checks must
  distinguish unavailable session, unavailable core and incompatible components;
  future hardware actions require a compatible core. Preserve diagnostic access.
- The Windows machine host exposes an authenticated, version/build-checked firmware
  command allowlist for qualified charge and performance policies. User-scoped
  audio/display controls and vendor-key capture belong to the resident session.
  Unknown native write outcomes block subsequent firmware writes pending reconciliation.
- The main UI is Redmibook Manager; Settings is one section. Left-clicking tray opens
  its mini-dashboard; right-click opens its menu. Its controls project observed device
  state and use the same qualified application ports as Manager. OSD belongs at the top left.
- Windows setup is elevated and machine-wide: Program Files/Koba Production/Redmibook Manager.
  Owner-facing installation must use the visible installer wizard, not a silent
  PowerShell/console launcher. Explain components, progress and the next steps;
  follow docs/windows-installation.md. Reserve unattended mode for explicit automation.
  The GUI runs without elevation; privileged operations belong to the future service.
  Keep writable data outside Program Files. Windows acceptance is the current priority.
- Define shared interfaces before Windows/Linux adapters; see docs/component-contracts.md.
  Machine/session runtimes, OSD and optional tray/settings have separate lifetimes.
  Closing tray/settings must not stop input/actions/OSD. Do not fix a Windows service
  or executable count as the shared architecture. Follow docs/process-lifecycle.md
  for OS host mappings and A-to-B upgrades; same-build reinstall is insufficient.
- During UI design iteration, use focused build-tree previews and application-owned
  screenshots first. Do not rebuild/package/run the installer after every UI change.
  The owner reviews screenshots or a separately identified development tray. Keep the
  installed runtime intact until the owner agrees to finalize/install a batch.
  Use tools/render-previews.ps1 -TrayOnly -Build for isolated tray snapshots; it does
  not connect to hardware, take desktop focus or install anything. Complete runtime
  updates use the installer only after an explicit request to install/finalize.
  For these owner-requested local updates, unattended installation is authorized:
  use tools/install-dev.ps1, which invokes the installer EXE directly with UAC and /S,
  preserves settings and checks runtime compatibility. Do not require wizard clicks
  or show an elevated terminal helper. UAC consent remains controlled by Windows.
- **Keep verification proportional to the change.** During implementation, inspect the
  actual diff and the affected behavior. Do not repeatedly run full tests, lint,
  builds, installation checks or smoke suites after each edit. Do not add tests
  that merely recheck other tests, mirror implementation, or cover trivial reversible
  UI/documentation edits. A targeted check needs a concrete unresolved failure or risk.
- Run the final full `verify` workflow once, immediately before an owner-authorized
  commit. Do not start this final pass while the owner is still reviewing local UI
  iterations. Reuse results for the same revision; repeat only checks invalidated
  by subsequent changes or an observed failure. Publication of that verified revision
  does not require another local full pass; CI remains independent.
  Never disable gates or introduce blanket suppressions to pass. Decompose by
  responsibility, not by hiding statements.
- Maintain Windows and Linux compile paths. Changes to a public port include both adapters.
- Significant changes require an independent Reviewer pass. A delegated reviewer owns
  read-only review, never edits the author's implementation. Review one integrated revision.
- Use Issues for mutable tasks and findings; documentation owns durable contracts.
- Use `koba-ai-agent` for publication; a separate reviewer owns PR review/merge.
  Do not self-approve or self-merge. Never bypass provider branch restrictions.
- **Publication authorization:** local builds, tests, documentation and previews do
  not authorize Git mutations. The owner authorized the current checkpoint commit,
  local-Git push and alpha CI publication after documentation and final verification.
  Future publication still requires explicit owner authorization; preserve branch
  restrictions and the independent research branch.
- Verify UI using bounded application-owned smoke tests/screenshots; do not interact with
  unrelated windows or treat offscreen results as native hardware acceptance.
  The owner prohibits interactive desktop capture/input: use isolated diagnostic
  builds and application-owned screenshots without taking focus or controlling the
  mouse/keyboard. Do not ask the owner to perform checks covered by these tools.
