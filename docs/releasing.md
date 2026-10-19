# Build, branches and release policy

## Branches

- `main`: development integration; build/test/package CI on pushes and pull requests.
- `release`: explicit promotion of a reviewed, locally accepted revision. A push runs
  the same Windows/Linux matrix and, only after all jobs pass, publishes an alpha prerelease.
- `codex/*`: scoped agent branches. Local work and verification precede publication.
- Existing `research` is independent and preserved; no automatic integration.

The repository must actually receive a workflow before GitHub can execute it.
Creating YAML locally does not establish a passing server pipeline or branch protection.
The initial Hello World stage is accepted locally first. Do not push to `release`,
create remote tags or publish a release as a side effect of ordinary local verification.
The current owner gate also forbids commits and all remote mutations until the owner
has personally checked the app and explicitly authorizes the corresponding Git action.

## Version and immutable builds

`VERSION.txt` owns the application and package base version, currently `1.1.1`.
Only the owner authorizes a base-version change. No automatic major/minor/patch bump.

An alpha publication from `release` uses `v1.1.1-alpha.<workflow-run-number>`.
The suffix identifies an immutable build; the executable still reports `1.1.1`.
CI embeds the same run number as `MANAGER_RELEASE_SEQUENCE` for update comparison.
The default `MANAGER_RELEASE_CHANNEL` is alpha; stable must be selected explicitly
when the owner authorizes a stable publication.
Promoting to beta or stable requires an explicit owner decision and policy update.
A rerun must not overwrite existing published assets or move an existing tag.

## Required evidence

1. Full local verify, Debug/Release build and bounded GUI checks.
2. Portable extraction into a path with spaces, with development paths removed.
3. Installer/application/uninstaller check in an isolated test location.
4. Independent review and a known source commit; no pending source changes.
5. Windows/Linux CI build, test, deployment and package checks on that revision.
6. Package checksums and a source-commit link attached to the prerelease.

Unsigned development artifacts must be described as such. Linux headless checks do not
replace desktop/hardware acceptance; native battery and hotkey support require separate
target-device evidence. Never label a local-only verification as a successful CI release.
