"""Exercise the vendored checkdeps gate against forbidden includes."""

import pathlib
import subprocess
import sys
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
CHECKDEPS = ROOT / "tools/vendor/chromium_checkdeps/checkdeps.py"


class BoundaryTests(unittest.TestCase):
    def check_include(self, include: str) -> int:
        with tempfile.TemporaryDirectory() as directory:
            fixture = pathlib.Path(directory)
            subprocess.run(["git", "init", "--quiet", directory], check=True)
            (fixture / "domain").mkdir()
            (fixture / "DEPS").write_bytes((ROOT / "DEPS").read_bytes())
            (fixture / "domain/DEPS").write_bytes((ROOT / "domain/DEPS").read_bytes())
            (fixture / "domain/probe.cpp").write_text(f"#include {include}\n", encoding="utf-8")
            result = subprocess.run(
                [sys.executable, str(CHECKDEPS), "--root", directory],
                capture_output=True, text=True, check=False,
            )
            return result.returncode

    def test_domain_rejects_qt(self) -> None:
        self.assertNotEqual(self.check_include("<QObject>"), 0)

    def test_domain_rejects_outer_layer(self) -> None:
        self.assertNotEqual(self.check_include('"adapters/platform/host_info.hpp"'), 0)

    def test_domain_accepts_domain(self) -> None:
        self.assertEqual(self.check_include('"domain/system/host_platform.hpp"'), 0)


if __name__ == "__main__":
    unittest.main()
