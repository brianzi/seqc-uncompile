"""T5b.5: verify `--to=<stage>` short-circuit semantics.

Per IF-304 (now partially closed): `seqcc --to=lower` and
`seqcc --to=asm` must skip the back-end steps (assemble-opcodes,
check-limits, ELF link) and route the requested IR straight to
`-o`.  Pre-T5b.5 the same flags ran the full pipeline and then
discarded the ELF; post-T5b.5 the driver branches between the
three `AWGCompilerImpl::stepXxx` methods on `opts.toStage`.

This file asserts the *user-visible* short-circuit invariants.
The byte-equality of the produced asm / lowered IR against the
full-pipeline run is already covered by `test_seqcc_diff.py`;
here we only check that the short-circuited output is well-formed,
non-empty, and distinct in shape from the linked ELF.
"""

import os
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]


def find_seqcc() -> Path:
    candidates = [
        REPO_ROOT / "reconstructed" / "build" / "seqcc" / "seqcc",
        REPO_ROOT / "build" / "seqcc" / "seqcc",
    ]
    for c in candidates:
        if c.exists() and os.access(c, os.X_OK):
            return c
    raise FileNotFoundError("seqcc binary not found; build the seqcc target first")


# A trivial but non-empty source so all three pipelines have something to do.
SAMPLE_SOURCE = """\
wave w = ones(64);
playWave(w);
"""


class SeqccToStageTest(unittest.TestCase):
    """T5b.5: --to=<stage> short-circuit verification."""

    @classmethod
    def setUpClass(cls):
        cls.seqcc = find_seqcc()
        cls.tmpdir = tempfile.mkdtemp(prefix="seqcc_to_")
        cls.src = Path(cls.tmpdir) / "sample.seqc"
        cls.src.write_text(SAMPLE_SOURCE)

    def _run(self, *args):
        result = subprocess.run(
            [str(self.seqcc), *args],
            capture_output=True,
            timeout=10,
        )
        self.assertEqual(
            result.returncode,
            0,
            msg=(
                f"seqcc {' '.join(args)} returned {result.returncode}\n"
                f"stdout: {result.stdout!r}\nstderr: {result.stderr!r}"
            ),
        )
        return result

    def _compile(self, stage: str) -> bytes:
        out = Path(self.tmpdir) / f"out.{stage}"
        if out.exists():
            out.unlink()
        self._run("-march=HDAWG8", f"--to={stage}", str(self.src), "-o", str(out))
        self.assertTrue(out.exists(), f"--to={stage} did not produce {out}")
        return out.read_bytes()

    def test_to_link_produces_elf(self):
        """--to=link emits a non-trivial 32-bit ELF (ELFCLASS32)."""
        data = self._compile("link")
        self.assertGreater(len(data), 1024, "ELF unexpectedly small")
        self.assertEqual(data[:4], b"\x7fELF", "missing ELF magic")
        self.assertEqual(data[4], 1, "expected ELFCLASS32 (e_ident[EI_CLASS]=1)")

    def test_to_asm_produces_text_not_elf(self):
        """--to=asm emits assembler text, not an ELF."""
        data = self._compile("asm")
        self.assertGreater(len(data), 0, "asm output is empty")
        self.assertNotEqual(data[:4], b"\x7fELF",
                            "--to=asm should not emit an ELF")
        text = data.decode("utf-8", errors="replace")
        # The HDAWG8 asm always starts with `cwvf` (config-waveform)
        # for any non-empty playWave program.
        self.assertIn("cwvf", text,
                      f"asm output missing expected mnemonic; got: {text[:200]!r}")

    def test_to_lower_produces_json_not_elf(self):
        """--to=lower emits lowered-AST JSON, not an ELF."""
        data = self._compile("lower")
        self.assertGreater(len(data), 0, "lower output is empty")
        self.assertNotEqual(data[:4], b"\x7fELF",
                            "--to=lower should not emit an ELF")
        text = data.decode("utf-8", errors="replace").lstrip()
        # The lowered-AST dump is a JSON object/array stream.
        self.assertIn(text[:1], "{[",
                      f"lower output is not JSON-shaped; got: {text[:200]!r}")

    def test_short_circuit_payloads_differ_from_link(self):
        """The three stages produce distinct byte streams."""
        link = self._compile("link")
        asm = self._compile("asm")
        lower = self._compile("lower")
        self.assertNotEqual(link, asm)
        self.assertNotEqual(link, lower)
        self.assertNotEqual(asm, lower)
        # The short-circuit outputs should be much smaller than the ELF.
        self.assertLess(len(asm), len(link),
                        "asm payload should be smaller than full ELF")
        self.assertLess(len(lower), len(link),
                        "lower payload should be smaller than full ELF")


if __name__ == "__main__":
    try:
        find_seqcc()
    except FileNotFoundError as e:
        print(f"SKIP: {e}", file=sys.stderr)
        sys.exit(0)
    unittest.main()
