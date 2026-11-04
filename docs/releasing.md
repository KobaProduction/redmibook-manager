# Build, branches and release policy

## Branches

- `main`: development integration; build/test/package CI on pushes and pull requests.
- `release`: explicit promotion of a reviewed, locally accepted revision. A push runs
  the same Windows/Linux matrix and, only after all jobs pass, publishes a release.
- `codex/*`: scoped agent branches. Local work and verification precede publication.
- Existing `research` is independent and preserved; no automatic integration.

The tracked workflow is .github/workflows/ci.yml. Its presence does not establish
a passing server pipeline or branch protection. Local builds/installations never
authorize publication by themselves. Publish or promote only after explicit owner
authorization and the required local checks; preserve remote branch restrictions.
An owner-requested publication promotes the same verified source commit to
release. The workflow creates its immutable tag only after the matrix succeeds.

## Version and immutable builds

Git is the sole version authority. `tools/git-version.py` is shared by every CMake
configuration, local packaging and CI. There is no editable version file, local
build counter, timestamp identity or CI-run-number version override.

The nearest first-parent version tag supplies the release series. Each subsequent
first-parent commit advances patch once. Versions and new tags use only
`MAJOR.MINOR.PATCH` and `vMAJOR.MINOR.PATCH`, without alpha/beta suffixes or a second
counter. Historical alpha tags remain valid numeric baselines: one commit after
v1.1.5-alpha.13 resolves to 1.1.6. Major/minor changes remain explicit owner decisions.
Tagging the computed version does not change its version or SHA.
Repeated builds never advance anything. Complete history and version tags are required;
missing/shallow Git metadata fails explicitly instead of inventing a version.

Payload identity is the commit SHA, plus a content hash for uncommitted tracked or
untracked files. Staging does not change identity; changing bytes does. Ignored build
outputs do not participate. Components compare both numeric version and this exact
payload identity. UI shows the Git release label; About/runtime diagnostics expose
the payload identity. Local package filenames use `-dirty-<12 hex characters>`:
one shortened SHA-256 of the complete payload identity, covering both the commit
and uncommitted content. For example,
`RedmibookManager-1.2.0-dirty-0123456789ab-x86_64.exe`.
Windows installers omit the redundant OS name; archives retain their platform.
Clean release filenames keep the version and architecture without the dirty suffix.
The full identity remains in the manifest and is used for compatibility checks.

`build-identity.json` is generated with the binaries and installed beside them.
Each package has a matching `.identity.json` sidecar. Installer helpers read this
metadata and verify the installed core/session identity, rather than guessing a
version from the source checkout or filename. CI fetches complete history/tags,
checks all package manifests against its source SHA and publishes their computed
release tag. Dirty payloads cannot be published. A rerun must not overwrite assets
or move an existing tag. No commit/publication is authorized merely by a local build.

## Required evidence

1. Full local verify, Debug/Release build and bounded GUI checks.
2. Portable extraction into a path with spaces, with development paths removed.
3. Installer/application/uninstaller check in an isolated test location.
4. Independent review and a known source commit; no pending source changes.
5. Windows/Linux CI build, test, deployment and package checks on that revision.
6. Package checksums and a source-commit link attached to the release.

Unsigned development artifacts must be described as such. Linux headless checks do not
replace desktop/hardware acceptance; native battery and hotkey support require separate
target-device evidence. Never label a local-only verification as a successful CI release.
