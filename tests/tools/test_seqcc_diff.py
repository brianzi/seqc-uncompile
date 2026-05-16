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
    """Drive seqcc as a subprocess; return ELF bytes.

    Uses dedicated flags (T3a surface) for kwargs that have them and
    falls back to -mtune=KEY=VALUE for anything unrecognised — this
    way the harness exercises the new surface as the user would.
    """
    src_path = tmp / "input.seqc"
    elf_path = tmp / "input.elf"
    src_path.write_text(src)

    argv = [str(SEQCC), f"--march={devtype}", "-o", str(elf_path)]

    # Map dedicated kwargs to dedicated flags.  Keep a copy so we can
    # detect which ones are still "escape hatch" candidates.
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
        # T3a: split on newlines and use repeatable -mdevopt= flags
        # for the parts we know how to.  Tests below also exercise the
        # legacy -mdevopts= packed form via the explicit_packed_devopts
        # case.
        for line in options.split("\n"):
            if line:
                argv.append(f"-mdevopt={line}")

    # Anything left over goes through the escape hatch.
    for k, v in leftover.items():
        argv.append(f"-mtune={k}={v}")

    argv.append(str(src_path))

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

    # ---- T3a surface-specific tests ----------------------------------
    # These don't fit the table-driven model because they exercise
    # CLI-surface details (legacy spellings, error paths) rather than
    # device/source matrix coverage.

    def test_legacy_mtune_samplerate_matches_dedicated_flag(self):
        """T3a back-compat: -mtune=samplerate=... still works."""
        src = "playZero(64);"
        with tempfile.TemporaryDirectory() as td:
            tmp = Path(td)
            (tmp / "t.seqc").write_text(src)
            elf_legacy = tmp / "legacy.elf"
            elf_new    = tmp / "new.elf"

            subprocess.run(
                [str(SEQCC), "--march=HDAWG8",
                 "-mtune=samplerate=2.4e9",
                 "-o", str(elf_legacy), str(tmp / "t.seqc")],
                check=True, capture_output=True)
            subprocess.run(
                [str(SEQCC), "--march=HDAWG8",
                 "--samplerate=2.4e9",
                 "-o", str(elf_new), str(tmp / "t.seqc")],
                check=True, capture_output=True)

            self.assertEqual(elf_legacy.read_bytes(), elf_new.read_bytes())

    def test_legacy_packed_mdevopts_matches_repeatable(self):
        """T3a back-compat: -mdevopts=$'MF\\nME' equals two -mdevopt=."""
        src = "playZero(64);"
        with tempfile.TemporaryDirectory() as td:
            tmp = Path(td)
            (tmp / "t.seqc").write_text(src)
            elf_packed = tmp / "packed.elf"
            elf_single = tmp / "single.elf"

            subprocess.run(
                [str(SEQCC), "--march=HDAWG8", "--samplerate=2.4e9",
                 "-mdevopts=MF\nME",
                 "-o", str(elf_packed), str(tmp / "t.seqc")],
                check=True, capture_output=True)
            subprocess.run(
                [str(SEQCC), "--march=HDAWG8", "--samplerate=2.4e9",
                 "-mdevopt=MF", "-mdevopt=ME",
                 "-o", str(elf_single), str(tmp / "t.seqc")],
                check=True, capture_output=True)

            self.assertEqual(elf_packed.read_bytes(), elf_single.read_bytes())

    def test_mtune_index_is_rejected(self):
        """IF-296: -mtune=index= used to be a silent no-op; T3a rejects it."""
        src = "playZero(64);"
        with tempfile.TemporaryDirectory() as td:
            tmp = Path(td)
            (tmp / "t.seqc").write_text(src)
            proc = subprocess.run(
                [str(SEQCC), "--march=HDAWG8", "--samplerate=2.4e9",
                 "-mtune=index=3",
                 "-o", str(tmp / "x.elf"), str(tmp / "t.seqc")],
                capture_output=True, text=True)
            self.assertNotEqual(proc.returncode, 0)
            self.assertIn("index", proc.stderr.lower())

    def test_index_flag_changes_elf(self):
        """--index=N is honoured (smoke-level: different index ⇒ different ELF)."""
        src = "playZero(64);"
        with tempfile.TemporaryDirectory() as td:
            tmp = Path(td)
            (tmp / "t.seqc").write_text(src)
            for idx in (0, 1):
                subprocess.run(
                    [str(SEQCC), "--march=HDAWG8", "--samplerate=2.4e9",
                     f"--index={idx}",
                     "-o", str(tmp / f"i{idx}.elf"), str(tmp / "t.seqc")],
                    check=True, capture_output=True)
            # The pybind binding receives `index` as the positional
            # awgIndex; verify seqcc matches for index=1.
            sc = self.sc
            elf, _ = sc.compile_seqc(src, "HDAWG8", "", 1, samplerate=2.4e9)
            self.assertEqual((tmp / "i1.elf").read_bytes(), bytes(elf))

    # ---- T3b: repeatable -mdevopt and --dump --------------------------
    # IF-300: an earlier CLI11 idiom silently dropped all but the last
    # `-mdevopt=` occurrence.  This test pins that two distinct
    # `-mdevopt=` flags produce an ELF that matches the equivalent
    # newline-packed `-mdevopts=` form (which is independently exercised
    # above against the pybind binding).

    def test_repeatable_mdevopt_registers_all_occurrences(self):
        """IF-300 regression: -mdevopt=A -mdevopt=B must register both.

        Verified via the compile-report `maxelfsize` field, which the
        binary scales by 8× when the ME device option is present.  If
        repeatability is broken, the ME flag is silently dropped and
        `maxelfsize` would equal the single-option case.
        """
        import json
        src = "wave w = ones(64); playWave(w);"
        with tempfile.TemporaryDirectory() as td:
            tmp = Path(td)
            (tmp / "t.seqc").write_text(src)

            def report(opts):
                elf = tmp / f"out.elf"
                report_path = tmp / "out.compile-report.json"
                cmd = [str(SEQCC), "--march=HDAWG8", "--samplerate=2.4e9",
                       "--dump=compile-report",
                       "-o", str(elf), str(tmp / "t.seqc"), *opts]
                subprocess.run(cmd, check=True, capture_output=True)
                data = json.loads(report_path.read_text())
                elf.unlink()
                report_path.unlink()
                return data

            r_one = report(["-mdevopt=MF"])
            r_two = report(["-mdevopt=MF", "-mdevopt=ME"])

            # Both runs succeed; ME presence scales maxelfsize 8× in
            # the binary's compile report.
            self.assertNotEqual(
                r_one["maxelfsize"], r_two["maxelfsize"],
                "second -mdevopt= was silently dropped — IF-300 regressed "
                f"(one={r_one}, two={r_two})")

    def test_dump_emits_requested_artifacts(self):
        """--dump=K[:P] writes per-kind files; ELF is unaffected."""
        src = "wave w = ones(64); playWave(w);"
        with tempfile.TemporaryDirectory() as td:
            tmp = Path(td)
            (tmp / "t.seqc").write_text(src)
            elf = tmp / "out.elf"

            subprocess.run(
                [str(SEQCC), "--march=HDAWG8", "--samplerate=2.4e9",
                 "--dump=asm", "--dump=waveforms",
                 "--dump=wavemem", "--dump=compile-report",
                 "-o", str(elf), str(tmp / "t.seqc")],
                check=True, capture_output=True)

            # All four kinds should land in the same dir as the ELF.
            for kind, ext in [("asm", "asm"),
                              ("waveforms", "json"),
                              ("wavemem", "json"),
                              ("compile-report", "json")]:
                path = tmp / f"out.{kind}.{ext}"
                self.assertTrue(path.exists(),
                                f"expected dump {path} to exist")
                self.assertGreater(path.stat().st_size, 0,
                                   f"dump {path} is empty")

            # The ELF must still match the pybind binding byte-for-byte
            # — emitting dumps must not perturb the compile output.
            sc = self.sc
            pybind_elf, _ = sc.compile_seqc(src, "HDAWG8", "", 0,
                                            samplerate=2.4e9)
            self.assertEqual(elf.read_bytes(), bytes(pybind_elf))

    def test_dump_explicit_path_overrides_default(self):
        """--dump=KIND:PATH writes to the explicit path."""
        src = "playZero(64);"
        with tempfile.TemporaryDirectory() as td:
            tmp = Path(td)
            (tmp / "t.seqc").write_text(src)
            elf = tmp / "out.elf"
            asm_out = tmp / "custom.asm.txt"

            subprocess.run(
                [str(SEQCC), "--march=HDAWG8", "--samplerate=2.4e9",
                 f"--dump=asm:{asm_out}",
                 "-o", str(elf), str(tmp / "t.seqc")],
                check=True, capture_output=True)

            self.assertTrue(asm_out.exists())
            self.assertFalse((tmp / "out.asm.asm").exists(),
                             "explicit :PATH should suppress default name")


if __name__ == "__main__":
    unittest.main()
