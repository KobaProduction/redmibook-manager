"""Exercise analysis reuse and invalidation with real file contents."""

import importlib.util
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.dont_write_bytecode = True
spec = importlib.util.spec_from_file_location('tidy_cache', Path(__file__).parents[1] / 'tools/tidy_cache.py')
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


class TidyCacheTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix='redmibook-tidy-')
        self.addCleanup(self.temporary.cleanup)
        self.build = Path(self.temporary.name)
        self.tool = self.build / 'clang-tidy'
        self.tool.write_bytes(b'analyzer fixture')
        self.source = self.build / 'source.cpp'
        self.header = self.build / 'nested.hpp'
        self.source.write_text('#include "nested.hpp"\n', encoding='utf-8')
        self.header.write_text('initial', encoding='utf-8')
        self.command = {'file': str(self.source), 'directory': str(self.build),
                        'output': 'source.obj', 'command': 'compiler -c source.cpp'}
        (self.build / 'CMakeCache.txt').write_text('CMAKE_MAKE_PROGRAM:FILEPATH=ninja\n', encoding='utf-8')
        self.dependencies = f'source.obj: #deps 2, deps mtime 1 (VALID)\n    {self.source}\n    {self.header}\n\n'
        self.save_commands()

    def save_commands(self):
        (self.build / 'compile_commands.json').write_text(json.dumps([self.command]), encoding='utf-8')

    def cache(self, arguments=()):
        with patch.object(module.subprocess, 'run', return_value=subprocess.CompletedProcess([], 0, self.dependencies)), \
                patch.object(module.subprocess, 'check_output', return_value=b'clang-tidy 19'):
            return module.AnalysisCache(str(self.tool), self.build, arguments)

    def remember(self):
        cache = self.cache()
        cache.record(self.source, cache.key(self.source), True)

    def assert_miss(self, cache):
        self.assertFalse(cache.hit(self.source, cache.key(self.source)))

    def test_identical_inputs_reuse_persisted_success(self):
        self.remember()
        cache = self.cache()
        self.assertTrue(cache.hit(self.source, cache.key(self.source)))

    def test_changed_source_and_transitive_header_invalidate(self):
        for file in (self.source, self.header):
            with self.subTest(file=file.name):
                self.remember()
                file.write_text('changed bytes', encoding='utf-8')
                self.assert_miss(self.cache())

    def test_flags_and_analyzer_configuration_invalidate(self):
        self.remember()
        self.assert_miss(self.cache(['--extra-arg=-DNEW=1']))
        self.command['command'] += ' -DNEW=1'
        self.save_commands()
        self.assert_miss(self.cache())
        self.remember()
        (self.build / '.clang-tidy').write_text('Checks: bugprone-*', encoding='utf-8')
        self.assert_miss(self.cache())

    def test_failed_analysis_never_reuses_old_success(self):
        self.remember()
        cache = self.cache()
        cache.record(self.source, cache.key(self.source), False)
        self.assert_miss(self.cache())

    def test_missing_dependency_or_metadata_disables_reuse(self):
        self.remember()
        recorded = self.dependencies
        self.dependencies = ''
        self.assertIsNone(self.cache().key(self.source))
        self.dependencies = recorded.replace('(VALID)', '(STALE)')
        self.assertIsNone(self.cache().key(self.source))
        self.dependencies = recorded
        self.header.unlink()
        self.assertIsNone(self.cache().key(self.source))

    def test_analyzer_binary_change_invalidates_same_version(self):
        self.remember()
        self.tool.write_bytes(b'new binary with same version label')
        self.assert_miss(self.cache())

    def test_multiple_compile_contexts_all_participate(self):
        other = {**self.command, 'output': 'other.obj', 'command': 'compiler -DOTHER=1 -c source.cpp'}
        commands = self.build / 'compile_commands.json'
        commands.write_text(json.dumps([self.command, other]), encoding='utf-8')
        self.dependencies += self.dependencies.replace('source.obj:', 'other.obj:')
        self.remember()
        cache = self.cache()
        self.assertTrue(cache.hit(self.source, cache.key(self.source)))
        other['command'] += ' -DCHANGED=1'
        commands.write_text(json.dumps([self.command, other]), encoding='utf-8')
        self.assert_miss(self.cache())


if __name__ == '__main__':
    unittest.main()
