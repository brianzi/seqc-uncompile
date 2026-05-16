#!/usr/bin/env python3
# tests/tools/test_seqcc_ab.py
#
# Phase T5a.3 A/B acceptance test for the SeqcDriver path.
#
# CMake builds two seqcc binaries from the same source list:
#   - `seqcc`        — gated by the `SEQCC_USE_OWNED_DRIVER` option
#                      (default OFF through T5a.2; routes through
#                      `compileSeqcWithIR` unless flipped).
#   - `seqcc_owned`  — always defines `SEQCC_USE_OWNED_DRIVER=1` and
#                      routes through `SeqcDriver::compile()`.
#
# This test invokes both binaries on the same curated case list and
# asserts the ELFs they produce are byte-identical.  Catches any
# drift between the legacy path and the owned-driver path as T5b
# lands the stepwise-API refactor, until T10a retires the legacy
# path entirely.
#
# The case list deliberately mirrors `test_seqcc_diff.py::CASES` so
# the A/B suite scales with the same device-family coverage we
# already trust for the seqcc-vs-pybind comparison.  No new fixtures
# are introduced here — the existing matrix is already comprehensive.

from __future__ import annotations

import os
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPO))
sys.path.insert(0, str(REPO / "tests"))

BUILD = REPO / "reconstructed" / "build"
SEQCC = BUILD / "seqcc" / "seqcc"
SEQCC_OWNED = BUILD / "seqcc" / "seqcc_owned"

# Reuse the curated case list from the seqcc-vs-pybind diff test so
# the A/B suite stays in lockstep with the canonical coverage matrix.
# Import-by-path rather than as a module so we don't drag in the
# full unittest TestCase from that file.
import importlib.util as _ilu

_spec = _ilu.spec_from_file_location(
    "_seqcc_diff_for_ab",
    Path(__file__).with_name("test_seqcc_diff.py"))
assert _spec and _spec.loader
_seqcc_diff = _ilu.module_from_spec(_spec)
_spec.loader.exec_module(_seqcc_diff)
CASES = _seqcc_diff.CASES


def _run_seqcc(binary: Path, tmp: Path, src: str, devtype: str,
               kwargs: dict, options: str) -> bytes:
    """Drive a seqcc-flavoured binary as a subprocess; return ELF bytes.

    Mirrors `_seqcc_compile` from `test_seqcc_diff.py` but
    parametrises over which binary to run, so the same harness can
    drive both `seqcc` and `seqcc_owned` against the same inputs.
    Any change to the flag-translation logic here must be mirrored
    in the diff test (and vice versa).
    """
    src_path = tmp / "input.seqc"
    elf_path = tmp / f"{binary.name}.elf"
    src_path.write_text(src)

    argv = [str(binary), f"--march={devtype}", "-o", str(elf_path)]

    leftover = dict(kwargs)
    if "sequencer" in leftover:
        argv.append(f"--sequencer={leftover.pop('sequencer')}")
    if "samplerate" in leftover:
        argv.append(f"--samplerate={leftover.pop('samplerate')}")
    if "filename" in leftover:
        argv.append(f"--filename={leftover.pop('filename')}")
    if "wavepath" in leftover:
        argv.append(f"--wave-path={leftover.pop('wavepath')}")
    if "waveforms" in leftover:
        argv.append(f"--waveforms={leftover.pop('waveforms')}")

    if options:
        for line in options.split("\n"):
            if line:
                argv.append(f"-mdevopt={line}")

    for k, v in leftover.items():
        argv.append(f"-mtune={k}={v}")

    argv.append(str(src_path))

    proc = subprocess.run(argv, capture_output=True, text=True)
    if proc.returncode != 0:
        raise AssertionError(
            f"{binary.name} failed (exit {proc.returncode})\n"
            f"argv: {argv}\nstdout: {proc.stdout}\nstderr: {proc.stderr}")
    return elf_path.read_bytes()


@unittest.skipUnless(SEQCC.exists() and SEQCC_OWNED.exists(),
                     f"seqcc / seqcc_owned not built at {SEQCC.parent}; "
                     f"run `cmake --build . --target seqcc seqcc_owned` "
                     f"from the build directory")
class SeqccAbTest(unittest.TestCase):
    """For each curated case, assert seqcc and seqcc_owned produce
    byte-identical ELFs.  Any drift indicates the SeqcDriver outer
    flow has diverged from the legacy `compileSeqcWithIR` path."""

    def _check(self, name, devtype, src, kwargs, options):
        with tempfile.TemporaryDirectory() as td:
            tmp = Path(td)
            legacy = _run_seqcc(SEQCC, tmp, src, devtype, kwargs, options)
            owned  = _run_seqcc(SEQCC_OWNED, tmp, src, devtype, kwargs, options)
        self.assertEqual(
            legacy, owned,
            f"{name}: legacy seqcc ELF ({len(legacy)} B) differs from "
            f"seqcc_owned ELF ({len(owned)} B) — SeqcDriver drift!")

    # Generate one test method per case so unittest reports each
    # independently — mirrors the loop in test_seqcc_diff.py.
    for _entry in CASES:
        _name, _dev, _src, _kw, _opts = _entry

        def _make(name=_name, dev=_dev, src=_src, kw=_kw, opts=_opts):
            def _test(self):
                self._check(name, dev, src, kw, opts)
            _test.__name__ = f"test_{name}"
            return _test

        locals()[f"test_{_name}"] = _make()
    if CASES:
        del _entry, _name, _dev, _src, _kw, _opts


if __name__ == "__main__":
    unittest.main()
