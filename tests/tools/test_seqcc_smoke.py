"""Smoke test for the seqcc toolchain driver.

Verifies that the freshly-built `seqcc` executable can be invoked and
responds to --help, --version, and basic error paths.  Pipeline-level
byte-equality assertions live in `tests/tools/test_seqcc_diff.py`.

Locates the binary by walking up from this file to the repo root and
looking under common build-directory names.  No assumptions about the
working directory.
"""

import os
import subprocess
import sys
import unittest
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]


def find_seqcc() -> Path:
    """Return the first existing seqcc binary, searching common build dirs."""
    candidates = [
        REPO_ROOT / "reconstructed" / "build" / "seqcc" / "seqcc",
        REPO_ROOT / "build" / "seqcc" / "seqcc",
        REPO_ROOT / "reconstructed" / "build_release" / "seqcc" / "seqcc",
    ]
    for c in candidates:
        if c.exists() and os.access(c, os.X_OK):
            return c
    raise FileNotFoundError(
        "Could not find seqcc binary; tried: "
        + ", ".join(str(c) for c in candidates)
        + ".  Build it with `cmake --build reconstructed/build --target seqcc`."
    )


class SeqccSmokeTest(unittest.TestCase):
    """T1: minimal end-to-end checks of the new driver binary."""

    @classmethod
    def setUpClass(cls):
        cls.seqcc = find_seqcc()

    def _run(self, *args, expect_returncode=0):
        result = subprocess.run(
            [str(self.seqcc), *args],
            capture_output=True,
            text=True,
            timeout=10,
        )
        self.assertEqual(
            result.returncode,
            expect_returncode,
            msg=(
                f"seqcc {' '.join(args)} returned {result.returncode}\n"
                f"stdout: {result.stdout}\nstderr: {result.stderr}"
            ),
        )
        return result

    def test_help_exits_zero(self):
        """`seqcc --help` exits 0 and prints usage."""
        r = self._run("--help")
        self.assertIn("seqcc", r.stdout.lower())

    def test_version_exits_zero_and_prints_version(self):
        """`seqcc --version` exits 0 and prints a version string."""
        r = self._run("--version")
        self.assertIn("seqcc", r.stdout.lower())
        self.assertRegex(r.stdout, r"\d+\.\d+\.\d+")

    def test_no_input_returns_nonzero(self):
        """Invoking with no arguments returns non-zero (usage hint)."""
        r = self._run(expect_returncode=1)
        self.assertIn("no input", r.stderr.lower())

    def test_missing_march_returns_two(self):
        """Passing an input file without -march is a usage error (T2)."""
        # CLI11 needs *something* on the positional; /dev/null is fine —
        # we never get to opening it because -march is missing.
        r = self._run("/dev/null", expect_returncode=2)
        self.assertIn("-march", r.stderr.lower())


if __name__ == "__main__":
    try:
        find_seqcc()
    except FileNotFoundError as e:
        print(f"SKIP: {e}", file=sys.stderr)
        sys.exit(0)
    unittest.main()
