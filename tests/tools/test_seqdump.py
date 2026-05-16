"""Phase T9 — `seqdump` personality tests.

`seqdump` is the ELF section dumper, invoked either as the
`seqdump` symlink to the `seqcc` binary or as `seqcc dump …` (not
yet wired).  It's a C++ port of `tests/dump_elf.py` with the same
section heuristics (decoded for `.text`, `.linenr`, `.wf_*`,
`.channels`, `.version_bin`; printed as text when bytes look
printable; hex-dumped otherwise) and a `--diff` mode for two-ELF
side-by-side comparison.
"""

from __future__ import annotations

import os
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
BUILD = REPO / "reconstructed" / "build"
SEQCC = BUILD / "seqcc" / "seqcc"
SEQDUMP = BUILD / "seqcc" / "seqdump"


def _have_binaries() -> bool:
    return (SEQCC.exists() and os.access(SEQCC, os.X_OK)
            and SEQDUMP.exists() and os.access(SEQDUMP, os.X_OK))


@unittest.skipUnless(_have_binaries(),
                     "seqcc/seqdump binaries not built — run "
                     "`cmake --build reconstructed/build --target seqcc`")
class T9SeqdumpBasics(unittest.TestCase):
    """End-to-end checks of the seqdump personality."""

    DEV_ARGS = ["--march=HDAWG8", "--samplerate=2.4e9"]
    SRC = "wave w = ones(64);\nrepeat (4) { playWave(w); }\n"

    @classmethod
    def setUpClass(cls):
        cls._tmpdir = tempfile.TemporaryDirectory()
        tmp = Path(cls._tmpdir.name)
        (tmp / "t.seqc").write_text(cls.SRC)
        cls.elf_default = tmp / "default.elf"
        cls.elf_o0 = tmp / "o0.elf"
        subprocess.run(
            [str(SEQCC), *cls.DEV_ARGS, "-o", str(cls.elf_default),
             str(tmp / "t.seqc")],
            check=True, capture_output=True)
        subprocess.run(
            [str(SEQCC), "-O0", *cls.DEV_ARGS, "-o", str(cls.elf_o0),
             str(tmp / "t.seqc")],
            check=True, capture_output=True)

    @classmethod
    def tearDownClass(cls):
        cls._tmpdir.cleanup()

    # --- Personality / metadata --------------------------------------

    def test_seqdump_is_symlink_to_seqcc(self):
        """The seqdump symlink resolves to the seqcc binary."""
        self.assertTrue(SEQDUMP.is_symlink(),
                        f"{SEQDUMP} should be a symlink")
        self.assertEqual(SEQDUMP.resolve(), SEQCC.resolve(),
                         "seqdump → seqcc symlink target mismatch")

    def test_version_exits_zero(self):
        """`seqdump --version` prints a version string and exits 0."""
        r = subprocess.run([str(SEQDUMP), "--version"],
                           capture_output=True, text=True)
        self.assertEqual(r.returncode, 0, r.stderr)
        self.assertIn("seqdump", r.stdout.lower())
        self.assertRegex(r.stdout, r"\d+\.\d+\.\d+")

    def test_help_exits_zero(self):
        """`seqdump --help` prints usage and exits 0."""
        r = subprocess.run([str(SEQDUMP), "--help"],
                           capture_output=True, text=True)
        self.assertEqual(r.returncode, 0, r.stderr)
        self.assertIn("usage", r.stdout.lower())
        self.assertIn("--diff", r.stdout)
        self.assertIn("--section", r.stdout)

    def test_no_args_exits_two(self):
        """`seqdump` with no positional argument is a usage error."""
        r = subprocess.run([str(SEQDUMP)],
                           capture_output=True, text=True)
        self.assertEqual(r.returncode, 2, r.stdout)
        self.assertIn("expected", r.stderr.lower())

    def test_unrecognised_option_exits_two(self):
        """Unknown long flag is a usage error."""
        r = subprocess.run([str(SEQDUMP), "--bogus", str(self.elf_default)],
                           capture_output=True, text=True)
        self.assertEqual(r.returncode, 2, r.stdout)
        self.assertIn("unrecognised", r.stderr.lower())

    # --- Single-ELF dump ---------------------------------------------

    def test_dump_default_lists_known_sections(self):
        """A full dump enumerates the sections we expect for HDAWG output."""
        r = subprocess.run([str(SEQDUMP), str(self.elf_default)],
                           capture_output=True, text=True)
        self.assertEqual(r.returncode, 0, r.stderr)
        out = r.stdout
        self.assertIn(f"=== {self.elf_default}", out)
        for expected in (".text", ".asm", ".waveforms", ".wavemem"):
            self.assertIn(expected, out,
                          f"dump missing section {expected!r}")
        # `.text` section header records instruction count.
        self.assertRegex(out, r"\.text: \d+ bytes = \d+ instructions")

    def test_dump_decodes_dot_asm_as_text(self):
        """`.asm` is plain text — assembly lines like `wvfe`, `wwvf`,
        `end` should appear verbatim in the dump."""
        r = subprocess.run([str(SEQDUMP), str(self.elf_default)],
                           capture_output=True, text=True, check=True)
        # `wwvf` and `end` always appear in any non-trivial HDAWG output.
        self.assertIn("wwvf", r.stdout)
        self.assertIn("end", r.stdout)

    def test_dump_decodes_dot_linenr_as_records(self):
        """`.linenr` is decoded as 16-byte records (abs/ctr/seq/line)."""
        r = subprocess.run([str(SEQDUMP), str(self.elf_default)],
                           capture_output=True, text=True, check=True)
        # If the ELF has a .linenr section, it must use the records
        # header format.
        if ".linenr" in r.stdout:
            self.assertRegex(r.stdout, r"\.linenr: \d+ bytes = \d+ records")

    def test_section_filter(self):
        """`--section=<name>` restricts output to a single section."""
        r = subprocess.run(
            [str(SEQDUMP), "--section=.text", str(self.elf_default)],
            capture_output=True, text=True, check=True)
        self.assertIn(".text", r.stdout)
        # Other large sections must not be present.
        self.assertNotIn(".asm:", r.stdout)
        self.assertNotIn(".waveforms:", r.stdout)

    def test_section_filter_repeatable(self):
        """`--section=` may be passed multiple times."""
        r = subprocess.run(
            [str(SEQDUMP), "--section=.text", "--section=.asm",
             str(self.elf_default)],
            capture_output=True, text=True, check=True)
        self.assertIn(".text", r.stdout)
        self.assertIn(".asm", r.stdout)
        self.assertNotIn(".waveforms:", r.stdout)

    def test_section_filter_unknown_warns_but_succeeds(self):
        """A non-existent section name produces a warning, not an error."""
        r = subprocess.run(
            [str(SEQDUMP), "--section=.no_such_section",
             str(self.elf_default)],
            capture_output=True, text=True)
        self.assertEqual(r.returncode, 0, r.stderr)
        self.assertIn("not present", r.stderr.lower())

    def test_dump_rejects_non_elf(self):
        """A non-ELF file is rejected with a clear diagnostic."""
        with tempfile.NamedTemporaryFile(suffix=".bin", delete=False) as f:
            f.write(b"not an elf\n")
            path = f.name
        try:
            r = subprocess.run([str(SEQDUMP), path],
                               capture_output=True, text=True)
            self.assertEqual(r.returncode, 1, r.stdout)
            self.assertIn("not a recognised seqc elf", r.stderr.lower())
        finally:
            os.unlink(path)

    # --- Diff mode ---------------------------------------------------

    def test_diff_identical_files_report_zero_differs(self):
        """Diffing an ELF against itself reports zero differing sections."""
        r = subprocess.run(
            [str(SEQDUMP), "--diff", str(self.elf_default),
             str(self.elf_default)],
            capture_output=True, text=True)
        self.assertEqual(r.returncode, 0, r.stderr)
        self.assertIn("=== 0 section(s) differ ===", r.stdout)
        # No DIFFERS / MISSING markers in identical case.
        self.assertNotIn("DIFFERS", r.stdout)
        self.assertNotIn("MISSING", r.stdout)

    def test_diff_different_files_report_some_differs(self):
        """Diffing default vs -O0 ELFs reports at least one difference."""
        r = subprocess.run(
            [str(SEQDUMP), "--diff", str(self.elf_o0),
             str(self.elf_default)],
            capture_output=True, text=True)
        self.assertEqual(r.returncode, 0, r.stderr)
        self.assertIn("DIFFERS", r.stdout)
        # Summary line carries a positive count.
        import re
        m = re.search(r"=== (\d+) section\(s\) differ ===", r.stdout)
        assert m is not None, "summary line missing"
        self.assertGreater(int(m.group(1)), 0,
                           "expected at least one differing section")

    def test_diff_shows_both_sides_with_labels(self):
        """Differing sections are printed with --- LEFT/RIGHT --- labels."""
        r = subprocess.run(
            [str(SEQDUMP), "--diff", str(self.elf_o0),
             str(self.elf_default)],
            capture_output=True, text=True, check=True)
        self.assertIn("--- LEFT", r.stdout)
        self.assertIn("--- RIGHT", r.stdout)

    def test_diff_section_filter_narrows_comparison(self):
        """`--section=<n>` honoured in diff mode."""
        r = subprocess.run(
            [str(SEQDUMP), "--section=.asm",
             "--diff", str(self.elf_o0), str(self.elf_default)],
            capture_output=True, text=True, check=True)
        # Only .asm gets reported (either DIFFERS or IDENTICAL).
        self.assertIn(".asm", r.stdout)
        self.assertNotIn(".text:", r.stdout)
        self.assertNotIn(".waveforms:", r.stdout)


if __name__ == "__main__":
    unittest.main()
