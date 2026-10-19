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
  reads [release policy](docs/releasing.md). Keep base version 1.1.1 until the owner changes it.
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

- **Serena MCP is mandatory for code work, including every spawned subagent.**
  Before reading/editing/refactoring code, call Serena `initial_instructions`, activate
  this checkout with its C++ language server, and use its semantic symbol/reference
  tools for code navigation and refactoring where applicable. File/shell tools remain
  appropriate for new files, configuration, builds and operations Serena does not support.
  Do not silently replace semantic code operations with text edits when Serena is absent,
  broken or unavailable. Stop the affected code work, explain the limitation, and obtain
  explicit permission from the user before proceeding without Serena. Restore/retry
  Serena first when possible; tool failure is not permission to bypass this rule.
- Every subagent task involving code or code review must explicitly repeat the Serena
  requirement and the stop-and-request-user-permission rule. A parent having read the
  manual does not exempt a subagent from initializing/validating its own Serena access.
- Domain/application are standard C++ only; inject OS services through application ports.
- Native queries/commands must not block the GUI thread. Use completion events/signals,
  bounded asynchronous requests and explicit pending/failure/timeout results. Where a
  native API is synchronous, isolate it on a worker with safe dependency lifetimes.
  Do not use Sleep, fixed-delay retries or GUI event pumping to wait for hardware.
  A UI deadline never proves that the underlying operation was cancelled.
- QML is presentation. Hardware access belongs to platform adapters, assembled in app entry points.
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
  The GUI runs without elevation; privileged operations belong to the future service.
  Keep writable data outside Program Files. Windows acceptance is the current priority.
- Define shared interfaces before Windows/Linux adapters; see docs/component-contracts.md.
  Machine/session runtimes, OSD and optional tray/settings have separate lifetimes.
  Closing tray/settings must not stop input/actions/OSD. Do not fix a Windows service
  or executable count as the shared architecture. Follow docs/process-lifecycle.md
  for OS host mappings and A-to-B upgrades; same-build reinstall is insufficient.
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
- **Current owner gate:** until the owner has personally checked the Hello World app
  and explicitly authorizes the next Git action, keep all work local. Do not commit,
  push, create remote branches/tags/PRs, update remote Issues or publish releases.
  Local builds, tests, documentation and application previews remain authorized.
- Verify UI using bounded application-owned smoke tests/screenshots; do not interact with
  unrelated windows or treat offscreen results as native hardware acceptance.
