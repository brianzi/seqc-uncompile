#!/usr/bin/env python3
# tests/tools/test_seqcc_diff.py
#
# Phase T2 acceptance test for the `seqcc` toolchain driver.
#
# For a curated subset of the differential manifest, invokes seqcc as a
# subprocess and asserts the ELF it produces is byte-identical to what
# `compile_seqc(...)` returns from the Python binding.  Both code paths
# end up calling the same `zhinst::compileSeqc` free function, so this
# is in effect a regression test that seqcc's CLI→JSON-config
# translation matches the kwarg-merging logic of `pybind_seqc.cpp`.
#
# T2 coverage: a fixed list of ~15 cases hand-picked to span device
# families (HDAWG, SHFQA, SHFSG, SHFQC, UHF*) and the most common
# config keys (samplerate, sequencer, wavepath/waveforms, filename).
# A future sub-phase (T6?) will widen this to "all difftest cases" once
# the toolchain handles every flag combination the manifest uses.

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

# pybind binding lives in the cmake build tree alongside seqcc.
BUILD = REPO / "reconstructed" / "build"
SEQCC = BUILD / "seqcc" / "seqcc"
PYBIND_DIR = BUILD  # _seqc_compiler.so sits at the cmake binary root


def _import_pybind():
    # Match the import strategy used by tests/diff_test.py.
    sys.path.insert(0, str(PYBIND_DIR))
    import _seqc_compiler as sc  # type: ignore
    return sc


# Curated case list.  Each entry is (name, devtype, source, kwargs, options).
# kwargs map 1:1 to both Python `**kwargs` and seqcc `-mtune=k=v` flags.
# Cases were chosen because (a) they exist in the binary's regression
# suite, (b) they exercise distinct device-config code paths, and
# (c) they don't require -mtune keys outside the current allowlist.
CASES = [
    # (name, devtype, src, kwargs, options)
    ("hdawg8_nop",   "HDAWG8", "playZero(64);",
                              {"samplerate": 2.4e9}, ""),
    ("hdawg8_wave",  "HDAWG8", "wave w = ones(64);\nplayWave(w);\n",
                              {"samplerate": 2.4e9}, ""),
    ("hdawg4_wave",  "HDAWG4", "wave w = ones(64);\nplayWave(w);\n",
                              {"samplerate": 2.4e9}, ""),
    ("hdawg8_loop",  "HDAWG8",
        "wave w = ones(64);\nrepeat (10) { playWave(w); }\n",
        {"samplerate": 2.4e9}, ""),
    ("hdawg8_var",   "HDAWG8",
        "var i = 0;\nplayZero(64);\ni = i + 1;\n",
        {"samplerate": 2.4e9}, ""),
    ("shfqa4_qa",    "SHFQA4", "playZero(64);",
                              {"sequencer": "qa"}, ""),
    ("shfsg8_sg",    "SHFSG8", "playZero(64);",
                              {"sequencer": "sg"}, ""),
    ("shfqc_qa",     "SHFQC",  "playZero(64);",
                              {"sequencer": "qa"}, ""),
    ("shfqc_sg",     "SHFQC",  "playZero(64);",
                              {"sequencer": "sg"}, ""),
    ("uhfqa_nop",    "UHFQA",  "playZero(64);", {}, ""),
    ("uhfawg_nop",   "UHFAWG", "playZero(64);", {}, ""),
    ("uhfli_nop",    "UHFLI",  "playZero(64);", {}, ""),
    ("hdawg8_filename", "HDAWG8", "playZero(64);",
        {"samplerate": 2.4e9, "filename": "fake_source.seqc"}, ""),
    ("hdawg8_devopts_mf", "HDAWG8", "playZero(64);",
        {"samplerate": 2.4e9}, "MF"),
    ("hdawg8_devopts_me", "HDAWG8", "playZero(64);",
        {"samplerate": 2.4e9}, "ME"),
]


def _seqcc_compile(tmp: Path, src: str, devtype: str,
                   kwargs: dict, options: str) -> bytes:
    """Drive seqcc as a subprocess; return ELF bytes."""
    src_path = tmp / "input.seqc"
    elf_path = tmp / "input.elf"
    src_path.write_text(src)

    argv = [str(SEQCC), f"-march={devtype}", "-o", str(elf_path),
            str(src_path)]
    if options:
        argv.append(f"-mdevopts={options}")
    for k, v in kwargs.items():
        argv.append(f"-mtune={k}={v}")

    proc = subprocess.run(argv, capture_output=True, text=True)
    if proc.returncode != 0:
        raise AssertionError(
            f"seqcc failed (exit {proc.returncode})\n"
            f"argv: {argv}\nstdout: {proc.stdout}\nstderr: {proc.stderr}")
    return elf_path.read_bytes()


def _pybind_compile(sc, src: str, devtype: str,
                    kwargs: dict, options: str) -> bytes:
    """Call compile_seqc() directly via the pybind binding."""
    elf, _info = sc.compile_seqc(src, devtype, options, 0, **kwargs)
    return bytes(elf)


@unittest.skipUnless(SEQCC.exists(),
                     f"seqcc not built at {SEQCC}; run cmake --build first")
class SeqccDiffTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.sc = _import_pybind()

    def _check(self, name, devtype, src, kwargs, options):
        with tempfile.TemporaryDirectory() as td:
            tmp = Path(td)
            seqcc_elf = _seqcc_compile(tmp, src, devtype, kwargs, options)
        pybind_elf = _pybind_compile(self.sc, src, devtype, kwargs, options)
        self.assertEqual(
            seqcc_elf, pybind_elf,
            f"{name}: seqcc ELF ({len(seqcc_elf)} B) differs from "
            f"compile_seqc ELF ({len(pybind_elf)} B)")

    # Generate one test method per case so unittest reports each
    # independently — easier to triage than a single parametrised loop.
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
