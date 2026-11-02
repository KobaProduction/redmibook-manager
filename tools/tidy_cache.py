"""Reuse successful analysis only for identical compiler inputs and dependencies."""

import hashlib
import json
import os
from pathlib import Path
import subprocess


def canonical(path):
    return os.path.normcase(os.path.abspath(path))


def ninja_dependencies(text, build):
    records = {}
    current = None
    for line in text.splitlines():
        if ': #deps ' in line:
            name, description = line.rsplit(': #deps ', 1)
            current = canonical(build / name) if description.endswith('(VALID)') else None
            if current:
                records[current] = []
        elif current and line.startswith('    '):
            records[current].append(build / line.strip())
        elif line:
            current = None
    return records


class AnalysisCache:
    def __init__(self, tool, build, arguments):
        self.path = build / '.quality' / 'clang-tidy.json'
        self.hashes = {}
        self.entries = {}
        self.commands = {}
        self.dependencies = {}
        self.context = ''
        try:
            self.entries = json.loads(self.path.read_text(encoding='utf-8'))
            if not isinstance(self.entries, dict):
                self.entries = {}
        except (OSError, ValueError):
            pass
        try:
            commands = json.loads((build / 'compile_commands.json').read_text(encoding='utf-8'))
            for command in commands:
                source = canonical(command['file'])
                self.commands.setdefault(source, []).append(command)
            settings = (build / 'CMakeCache.txt').read_text(encoding='utf-8')
            ninja = next(line.split('=', 1)[1] for line in settings.splitlines()
                         if line.startswith('CMAKE_MAKE_PROGRAM:FILEPATH='))
            result = subprocess.run([ninja, '-C', str(build), '-t', 'deps'], capture_output=True,
                                    text=True, encoding='utf-8', errors='replace', check=True)
            self.dependencies = ninja_dependencies(result.stdout, build)
            root = Path(__file__).resolve().parents[1]
            headers = sorted(str(path.relative_to(root)) for folder in
                             ('application', 'domain', 'contracts', 'adapters', 'presentation', 'apps', 'tests')
                             for path in (root / folder).rglob('*') if path.suffix in ('.h', '.hpp'))
            version = subprocess.check_output([tool, '--version'])
            self.context = hashlib.sha256(version + Path(tool).read_bytes() + json.dumps(arguments).encode() +
                                          json.dumps(headers).encode() + Path(__file__).read_bytes() +
                                          Path(__file__).with_name('run-clang-tidy.py').read_bytes()).hexdigest()
        except (OSError, ValueError, KeyError, StopIteration, subprocess.SubprocessError):
            # Missing/unsupported compiler dependency metadata disables reuse, never analysis.
            self.dependencies = {}

    def digest(self, path):
        name = canonical(path)
        if name not in self.hashes:
            self.hashes[name] = hashlib.sha256(Path(path).read_bytes()).hexdigest()
        return self.hashes[name]

    def key(self, source):
        try:
            commands = self.commands[canonical(source)]
            dependencies = []
            for command in commands:
                output = Path(command['directory']) / command['output']
                dependencies.extend(self.dependencies[canonical(output)])
            if not dependencies:
                return None
            inputs = {canonical(path): self.digest(path) for path in [Path(source), *dependencies]}
            for directory in [Path(source).parent, *Path(source).parents]:
                config = directory / '.clang-tidy'
                inputs[str(config)] = self.digest(config) if config.is_file() else None
            return hashlib.sha256(json.dumps([self.context, commands, inputs], sort_keys=True).encode()).hexdigest()
        except (OSError, ValueError, KeyError):
            return None

    def hit(self, source, key):
        return key is not None and self.entries.get(canonical(source)) == key

    def record(self, source, key, success):
        self.entries.pop(canonical(source), None)
        if success and key:
            self.entries[canonical(source)] = key
        self.path.parent.mkdir(parents=True, exist_ok=True)
        temporary = self.path.with_suffix('.tmp')
        temporary.write_text(json.dumps(self.entries, sort_keys=True), encoding='utf-8')
        temporary.replace(self.path)
