# Build, branches and release policy

## Branches

- `main`: development integration; build/test/package CI on pushes and pull requests.
- `release`: explicit promotion of a reviewed, locally accepted revision. A push runs
  the same Windows/Linux matrix and, only after all jobs pass, publishes an alpha prerelease.
- `codex/*`: scoped agent branches. Local work and verification precede publication.
- Existing `research` is independent and preserved; no automatic integration.

The tracked workflow is .github/workflows/ci.yml. Its presence does not establish
a passing server pipeline or branch protection. Local builds/installations never
authorize publication by themselves. Publish or promote only after explicit owner
authorization and the required local checks; preserve remote branch restrictions.
An owner-requested alpha publication promotes the same verified source commit to
release. The workflow creates its immutable tag only after the matrix succeeds.

## Version and immutable builds

`VERSION.txt` owns the application and package base version, currently `1.1.5`.
Product versions identify checkpoint commits. A patch increment may be included
only in an owner-authorized checkpoint commit, at most once per checkpoint.
Uncommitted iterations, previews, builds, packaging and installations retain the
current version; distinguish their binaries with the payload build identity.
Do not commit solely to obtain a version bump or infer Git authorization from an
installation request. Major/minor changes require an explicit owner decision.
CI never bumps versions.

An alpha publication from `release` uses `v1.1.5-alpha.<workflow-run-number>`.
The suffix identifies an immutable build; the executable still reports `1.1.5`.
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
