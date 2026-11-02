"""Exercise versioning against isolated real Git histories, without Qt or publication."""
import importlib.util
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

sys.dont_write_bytecode = True
spec = importlib.util.spec_from_file_location("git_version", Path(__file__).parents[1] / "tools/git-version.py")
versioning = importlib.util.module_from_spec(spec)
spec.loader.exec_module(versioning)


class GitVersionTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory(prefix="redmibook-version-")
        self.addCleanup(self.directory.cleanup)
        self.source = Path(self.directory.name)
        self.git("init", "-q")
        self.git("config", "user.name", "Version Test")
        self.git("config", "user.email", "version-test@example.invalid")
        self.git("config", "core.autocrlf", "false")
        (self.source / "source.txt").write_text("initial\n", encoding="utf-8")
        self.git("add", ".")
        self.git("commit", "-qm", "initial")
        self.git("tag", "v1.2.3-alpha.7")

    def git(self, *arguments):
        return subprocess.check_output(["git", "-C", str(self.source), *arguments]).decode().strip()

    def identity(self):
        return versioning.resolve(self.source)["identity"]

    def test_clean_tag_and_repeat(self):
        identity = self.identity()
        self.assertEqual(identity["displayVersion"], "1.2.3-alpha.7")
        self.assertEqual(identity["build"], "g" + self.git("rev-parse", "HEAD"))
        self.assertFalse(identity["dirty"])
        self.assertEqual(identity, self.identity())

    def test_commit_advances_patch_and_tagging_preserves_identity(self):
        (self.source / "source.txt").write_text("next\n", encoding="utf-8")
        self.git("commit", "-qam", "next")
        before = self.identity()
        self.assertEqual(before["displayVersion"], "1.2.4-alpha.0")
        self.git("tag", before["releaseTag"])
        after = self.identity()
        for key in ("version", "build", "displayVersion", "releaseTag", "sequence"):
            self.assertEqual(before[key], after[key])

    def test_dirty_state_staging_and_untracked_changes(self):
        clean = self.identity()
        (self.source / "source.txt").write_text("edited\n", encoding="utf-8")
        edited = self.identity()
        self.assertNotEqual(edited["build"], clean["build"])
        self.assertLessEqual(len(edited["build"]), 96)
        self.git("add", ".")
        self.assertEqual(edited["build"], self.identity()["build"])
        (self.source / "new.cpp").write_text("new", encoding="utf-8")
        first = self.identity()
        (self.source / "new.cpp").write_text("changed", encoding="utf-8")
        self.assertNotEqual(first["build"], self.identity()["build"])

    def test_staging_new_files_and_rename_preserves_identity(self):
        (self.source / "new.cpp").write_text("new\n", encoding="utf-8")
        before = self.identity()["build"]
        self.git("add", ".")
        self.assertEqual(before, self.identity()["build"])
        (self.source / "source.txt").rename(self.source / "renamed.txt")
        renamed = self.identity()["build"]
        self.git("add", "-A")
        self.assertEqual(renamed, self.identity()["build"])

    def test_stable_tag(self):
        self.git("tag", "-d", "v1.2.3-alpha.7")
        self.git("tag", "v2.0.0")
        self.assertEqual(self.identity()["displayVersion"], "2.0.0")
        self.assertEqual(self.identity()["channel"], "stable")

    def test_missing_tag_fails(self):
        self.git("tag", "-d", "v1.2.3-alpha.7")
        with self.assertRaises(subprocess.CalledProcessError):
            self.identity()

    def test_shallow_history_fails(self):
        clone = self.source / 'shallow'
        subprocess.check_call(['git', 'clone', '-q', '--depth', '1', '--no-local', str(self.source), str(clone)])
        with self.assertRaises(ValueError):
            versioning.resolve(clone)


if __name__ == "__main__":
    unittest.main()
