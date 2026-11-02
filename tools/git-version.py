"""Resolve every build's identity from Git; never assign or write a version."""

import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys


def git(source, *arguments, binary=False):
    result = subprocess.run(
        ["git", "-C", str(source), *arguments], capture_output=True, check=True
    ).stdout
    return result if binary else result.decode("utf-8").strip()


def resolve(source):
    if git(source, "rev-parse", "--is-shallow-repository") != "false":
        raise ValueError("Versioning requires complete Git history and tags (fetch-depth: 0).")
    tag = git(source, "describe", "--first-parent", "--tags", "--match", "v[0-9]*.[0-9]*.[0-9]*", "--abbrev=0")
    match = re.fullmatch(r"v(\d+)\.(\d+)\.(\d+)(?:-alpha\.(\d+))?", tag)
    if not match:
        raise ValueError(f"Unsupported Git version tag: {tag}")
    revision = git(source, "rev-parse", "HEAD")
    distance = int(git(source, "rev-list", "--count", "--first-parent", f"{tag}..HEAD"))
    major, minor, patch = (int(match[index]) for index in (1, 2, 3))
    version = f"{major}.{minor}.{patch + distance}"
    channel = "stable" if distance == 0 and match[4] is None else "alpha"
    sequence = int(match[4]) if distance == 0 and match[4] else 0
    release_tag = f"v{version}" + (f"-alpha.{sequence}" if channel == "alpha" else "")
    diff = git(source, "diff", "--no-ext-diff", "--no-renames", "--diff-filter=CDMRTUXB", "--binary", "HEAD", binary=True)
    added = git(source, "diff", "--no-ext-diff", "--no-renames", "--diff-filter=A", "--name-only", "-z", "HEAD", binary=True)
    untracked = git(source, "ls-files", "--others", "--exclude-standard", "-z", binary=True)
    new_files = (set(added.split(b"\0")) | set(untracked.split(b"\0"))) - {b""}
    dirty = bool(diff or new_files)
    digest = hashlib.sha256(diff)
    for name in sorted(new_files):
        if name:
            path = source / name.decode("utf-8")
            content = path.read_bytes() if not path.is_symlink() else str(path.readlink()).encode("utf-8")
            digest.update(name + b"\0" + hashlib.sha256(content).digest())
    # Keep the full commit plus a 128-bit content fingerprint within the IPC's 96-character identity limit.
    dirty_hash = digest.hexdigest()[:32] if dirty else ""
    build = f"g{revision}" + (f".dirty.{dirty_hash}" if dirty else "")
    suffix = f"-g{revision[:12]}-dirty-{dirty_hash[:12]}" if dirty else ""
    files = git(source, "ls-files", "--cached", "--others", "--exclude-standard", "-z", binary=True)
    inputs = [str(source / name.decode("utf-8")) for name in files.split(b"\0") if name and (source / name.decode("utf-8")).exists()]
    for name in ("HEAD", "index", "packed-refs", f"refs/tags/{tag}"):
        path = source / git(source, "rev-parse", "--git-path", name)
        if path.exists():
            inputs.append(str(path))
    branch = subprocess.run(["git", "-C", str(source), "symbolic-ref", "-q", "HEAD"], capture_output=True)
    if branch.returncode == 0:
        inputs.append(str(source / git(source, "rev-parse", "--git-path", branch.stdout.decode().strip())))
    return {
        "identity": {"version": version, "displayVersion": release_tag[1:], "build": build,
                     "commit": revision, "baseTag": tag, "distance": distance, "dirty": dirty,
                     "channel": channel, "sequence": sequence, "releaseTag": release_tag,
                     "packageSuffix": suffix},
        "inputs": inputs,
        "gitDirectories": [str((source / git(source, "rev-parse", "--git-path", "packed-refs")).parent),
                           str(source / git(source, "rev-parse", "--git-path", "refs/tags"))],
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, required=True)
    arguments = parser.parse_args()
    try:
        print(json.dumps(resolve(arguments.source.resolve()), ensure_ascii=True))
    except (ValueError, OSError, subprocess.CalledProcessError) as error:
        print(f"Git version resolution failed: {error}. Fetch version tags; no manual fallback is used.", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
