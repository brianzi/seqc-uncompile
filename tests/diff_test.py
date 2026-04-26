#!/usr/bin/env python3
"""Differential test harness for SeqC compiler reconstruction.

Runs each test case through both the original and reconstructed
_seqc_compiler modules (in isolated subprocesses) and compares the
resulting ELF output section-by-section.

Usage:
    python3 diff_test.py [--cases-dir tests/cases] [--original-dir ..] \
                         [--recon-dir ../reconstructed/build]

See tests/README.md for setup instructions.
"""

import argparse
import hashlib
import json
import os
import struct
import subprocess
import sys
import tempfile
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any, Dict, List, Optional, Tuple


# ── ELF parsing (minimal, no pyelftools dependency) ────────────────────────

@dataclass
class ElfSection:
    name: str
    type: int
    flags: int
    addr: int
    offset: int
    size: int
    data: bytes

    @property
    def hex_digest(self) -> str:
        return hashlib.sha256(self.data).hexdigest()[:16]


@dataclass
class ElfInfo:
    """Minimal ELF parse — enough for section-level comparison."""
    raw: bytes
    e_type: int = 0
    e_machine: int = 0
    e_entry: int = 0
    sections: List[ElfSection] = field(default_factory=list)
    parse_error: Optional[str] = None

    @staticmethod
    def parse(data: bytes) -> "ElfInfo":
        info = ElfInfo(raw=data)
        if len(data) < 52:
            info.parse_error = f"too short ({len(data)} bytes)"
            return info
        if data[:4] != b"\x7fELF":
            info.parse_error = "not an ELF file"
            return info

        ei_class = data[4]  # 1=32-bit, 2=64-bit
        ei_data = data[5]   # 1=LE, 2=BE
        if ei_data != 1:
            info.parse_error = f"unsupported endianness {ei_data}"
            return info

        if ei_class == 1:
            return _parse_elf32(data, info)
        elif ei_class == 2:
            return _parse_elf64(data, info)
        else:
            info.parse_error = f"unknown EI_CLASS {ei_class}"
            return info


def _parse_elf32(data: bytes, info: ElfInfo) -> ElfInfo:
    hdr = struct.unpack_from("<HHIIIIIHHHHHH", data, 16)
    info.e_type, info.e_machine, _, info.e_entry = hdr[0], hdr[1], hdr[2], hdr[3]
    e_shoff = hdr[4]
    e_shentsize = hdr[9]
    e_shnum = hdr[10]
    e_shstrndx = hdr[11]

    if e_shoff == 0 or e_shnum == 0:
        return info

    # Read section headers
    shdrs = []
    for i in range(e_shnum):
        off = e_shoff + i * e_shentsize
        sh = struct.unpack_from("<IIIIIIIIII", data, off)
        shdrs.append(sh)

    # Read string table
    strtab_sh = shdrs[e_shstrndx]
    strtab = data[strtab_sh[4]:strtab_sh[4] + strtab_sh[5]]

    for sh in shdrs:
        name_off = sh[0]
        name = _read_cstr(strtab, name_off)
        sec_data = data[sh[4]:sh[4] + sh[5]] if sh[5] > 0 else b""
        info.sections.append(ElfSection(
            name=name, type=sh[1], flags=sh[2], addr=sh[3],
            offset=sh[4], size=sh[5], data=sec_data,
        ))
    return info


def _parse_elf64(data: bytes, info: ElfInfo) -> ElfInfo:
    hdr = struct.unpack_from("<HHIQQQIHHHHHH", data, 16)
    info.e_type, info.e_machine = hdr[0], hdr[1]
    info.e_entry = hdr[3]
    e_shoff = hdr[5]
    e_shentsize = hdr[9]
    e_shnum = hdr[10]
    e_shstrndx = hdr[11]

    if e_shoff == 0 or e_shnum == 0:
        return info

    shdrs = []
    for i in range(e_shnum):
        off = e_shoff + i * e_shentsize
        sh = struct.unpack_from("<IIQQQQIIQQ", data, off)
        shdrs.append(sh)

    strtab_sh = shdrs[e_shstrndx]
    strtab = data[strtab_sh[4]:strtab_sh[4] + strtab_sh[5]]

    for sh in shdrs:
        name = _read_cstr(strtab, sh[0])
        sec_data = data[sh[4]:sh[4] + sh[5]] if sh[5] > 0 else b""
        info.sections.append(ElfSection(
            name=name, type=sh[1], flags=sh[2], addr=sh[3],
            offset=sh[4], size=sh[5], data=sec_data,
        ))
    return info


def _read_cstr(buf: bytes, offset: int) -> str:
    end = buf.find(b"\x00", offset)
    if end == -1:
        end = len(buf)
    return buf[offset:end].decode("ascii", errors="replace")


# ── Test case loading ──────────────────────────────────────────────────────

@dataclass
class TestCase:
    name: str
    code: str           # SeqC source or "@filepath"
    devtype: str
    options: str = ""
    index: int = 0
    samplerate: Optional[float] = None
    sequencer: Optional[str] = None


def load_test_cases(cases_dir: Path) -> List[TestCase]:
    """Load test cases from a manifest.json file."""
    manifest = cases_dir / "manifest.json"
    if not manifest.exists():
        print(f"ERROR: {manifest} not found", file=sys.stderr)
        sys.exit(2)

    with open(manifest) as f:
        entries = json.load(f)

    cases = []
    for entry in entries:
        # If "file" key is present, read code from that file
        code = entry.get("code", "")
        if "file" in entry:
            code_path = cases_dir / entry["file"]
            if code_path.exists():
                code = f"@{code_path}"
            else:
                print(f"WARNING: {code_path} not found, skipping {entry.get('name')}")
                continue

        cases.append(TestCase(
            name=entry["name"],
            code=code,
            devtype=entry["devtype"],
            options=entry.get("options", ""),
            index=entry.get("index", 0),
            samplerate=entry.get("samplerate"),
            sequencer=entry.get("sequencer"),
        ))
    return cases


# ── Subprocess compilation ─────────────────────────────────────────────────

@dataclass
class CompileResult:
    elf_bytes: bytes
    meta: Dict[str, Any]
    error: Optional[str] = None


def run_compile(worker: Path, module_dir: Path, module_name: str,
                case: TestCase, tmpdir: str,
                label: str) -> CompileResult:
    """Run compile_worker.py in a subprocess, return results."""
    elf_path = os.path.join(tmpdir, f"{label}.elf")
    meta_path = os.path.join(tmpdir, f"{label}.json")

    cmd = [
        sys.executable, str(worker),
        "--module-dir", str(module_dir),
        "--module-name", module_name,
        "--code", case.code,
        "--devtype", case.devtype,
        "--options", case.options,
        "--index", str(case.index),
        "--output-elf", elf_path,
        "--output-meta", meta_path,
    ]
    if case.samplerate is not None:
        cmd += ["--samplerate", str(case.samplerate)]
    if case.sequencer is not None:
        cmd += ["--sequencer", case.sequencer]

    proc = subprocess.run(
        cmd, capture_output=True, text=True, timeout=60,
    )

    if proc.returncode == 2:
        return CompileResult(b"", {}, error=f"worker import error: {proc.stderr}")

    meta = {}
    if os.path.exists(meta_path):
        with open(meta_path) as f:
            meta = json.load(f)

    elf_bytes = b""
    if os.path.exists(elf_path):
        with open(elf_path, "rb") as f:
            elf_bytes = f.read()

    error = meta.get("error")
    if proc.returncode != 0 and not error:
        error = f"worker exit {proc.returncode}: {proc.stderr}"

    return CompileResult(elf_bytes, meta, error=error)


# ── Comparison ─────────────────────────────────────────────────────────────

@dataclass
class SectionDiff:
    section: str
    kind: str           # "missing_in_orig", "missing_in_recon", "size", "content"
    detail: str


@dataclass
class CompareResult:
    test_name: str
    passed: bool = False
    elf_size_orig: int = 0
    elf_size_recon: int = 0
    byte_identical: bool = False
    section_diffs: List[SectionDiff] = field(default_factory=list)
    orig_error: Optional[str] = None
    recon_error: Optional[str] = None
    notes: List[str] = field(default_factory=list)


def compare_results(name: str, orig: CompileResult,
                    recon: CompileResult) -> CompareResult:
    result = CompareResult(test_name=name)

    # Handle errors
    if orig.error and recon.error:
        # Both errored — compare error messages
        result.notes.append(f"both errored")
        result.orig_error = orig.error
        result.recon_error = recon.error
        result.passed = (orig.error == recon.error)
        if not result.passed:
            result.notes.append("error messages differ")
        return result

    if orig.error:
        result.orig_error = orig.error
        result.passed = False
        result.notes.append("original errored but recon succeeded")
        return result

    if recon.error:
        result.recon_error = recon.error
        result.passed = False
        result.notes.append("recon errored but original succeeded")
        return result

    # Both succeeded — compare ELF
    result.elf_size_orig = len(orig.elf_bytes)
    result.elf_size_recon = len(recon.elf_bytes)

    if orig.elf_bytes == recon.elf_bytes:
        result.byte_identical = True
        result.passed = True
        return result

    # Parse ELFs and compare section-by-section
    elf_o = ElfInfo.parse(orig.elf_bytes)
    elf_r = ElfInfo.parse(recon.elf_bytes)

    if elf_o.parse_error:
        result.notes.append(f"original ELF parse error: {elf_o.parse_error}")
    if elf_r.parse_error:
        result.notes.append(f"recon ELF parse error: {elf_r.parse_error}")

    if elf_o.parse_error or elf_r.parse_error:
        result.passed = False
        return result

    # Compare header fields
    if elf_o.e_type != elf_r.e_type:
        result.notes.append(f"e_type: orig={elf_o.e_type} recon={elf_r.e_type}")
    if elf_o.e_machine != elf_r.e_machine:
        result.notes.append(f"e_machine: orig={elf_o.e_machine} recon={elf_r.e_machine}")
    if elf_o.e_entry != elf_r.e_entry:
        result.notes.append(f"e_entry: orig=0x{elf_o.e_entry:x} recon=0x{elf_r.e_entry:x}")

    # Build section maps
    orig_secs = {s.name: s for s in elf_o.sections}
    recon_secs = {s.name: s for s in elf_r.sections}

    all_names = sorted(set(orig_secs) | set(recon_secs))
    for sname in all_names:
        if sname not in orig_secs:
            result.section_diffs.append(SectionDiff(
                sname, "missing_in_orig",
                f"size={recon_secs[sname].size}"))
            continue
        if sname not in recon_secs:
            result.section_diffs.append(SectionDiff(
                sname, "missing_in_recon",
                f"size={orig_secs[sname].size}"))
            continue

        so, sr = orig_secs[sname], recon_secs[sname]
        if so.data == sr.data:
            continue

        if so.size != sr.size:
            result.section_diffs.append(SectionDiff(
                sname, "size",
                f"orig={so.size} recon={sr.size}"))
        else:
            # Same size, different content — find first divergence
            for i, (a, b) in enumerate(zip(so.data, sr.data)):
                if a != b:
                    result.section_diffs.append(SectionDiff(
                        sname, "content",
                        f"first diff at offset 0x{i:x} "
                        f"(orig=0x{a:02x} recon=0x{b:02x}), "
                        f"section size={so.size}"))
                    break

    result.passed = len(result.section_diffs) == 0 and len(result.notes) == 0
    return result


# ── Reporting ──────────────────────────────────────────────────────────────

PASS = "\033[32mPASS\033[0m"
FAIL = "\033[31mFAIL\033[0m"
SKIP = "\033[33mSKIP\033[0m"


def print_result(r: CompareResult, verbose: bool = False):
    status = PASS if r.passed else FAIL
    extra = ""
    if r.byte_identical:
        extra = " (byte-identical)"
    elif r.passed:
        extra = ""
    elif r.orig_error or r.recon_error:
        extra = " (error)"

    print(f"  {status}  {r.test_name}{extra}")

    if not r.passed or verbose:
        if r.elf_size_orig or r.elf_size_recon:
            print(f"         ELF size: orig={r.elf_size_orig} recon={r.elf_size_recon}")
        if r.orig_error:
            print(f"         orig error: {r.orig_error[:120]}")
        if r.recon_error:
            print(f"         recon error: {r.recon_error[:120]}")
        for note in r.notes:
            print(f"         note: {note}")
        for sd in r.section_diffs:
            print(f"         section '{sd.section}': {sd.kind} — {sd.detail}")


# ── Main ───────────────────────────────────────────────────────────────────

def main():
    p = argparse.ArgumentParser(description="SeqC differential ELF test harness")
    p.add_argument("--cases-dir", type=Path,
                   default=Path(__file__).parent / "cases",
                   help="Directory containing manifest.json and .seqc files")
    p.add_argument("--original-dir", type=Path, default=None,
                   help="Directory containing the original _seqc_compiler.so "
                        "(default: repo root)")
    p.add_argument("--recon-dir", type=Path, default=None,
                   help="Directory containing the reconstructed _seqc_compiler.so "
                        "(default: reconstructed/build)")
    p.add_argument("--recon-module", default="_seqc_compiler",
                   help="Module name for the reconstruction (default: _seqc_compiler)")
    p.add_argument("--original-only", action="store_true",
                   help="Only run the original compiler (smoke test)")
    p.add_argument("-v", "--verbose", action="store_true")
    p.add_argument("--filter", default=None,
                   help="Only run test cases whose name contains this string")
    args = p.parse_args()

    # Resolve paths
    repo_root = Path(__file__).resolve().parent.parent
    worker = Path(__file__).resolve().parent / "compile_worker.py"

    if args.original_dir is None:
        args.original_dir = repo_root
    if args.recon_dir is None:
        args.recon_dir = repo_root / "reconstructed" / "build"

    # Load test cases
    cases = load_test_cases(args.cases_dir)
    if args.filter:
        cases = [c for c in cases if args.filter in c.name]

    if not cases:
        print("No test cases found.")
        return 1

    print(f"Running {len(cases)} test case(s)")
    print(f"  Original: {args.original_dir}")
    if not args.original_only:
        print(f"  Recon:    {args.recon_dir}")
    print()

    passed = 0
    failed = 0
    skipped = 0

    with tempfile.TemporaryDirectory(prefix="seqc_diff_") as tmpdir:
        for case in cases:
            if args.verbose:
                print(f"  ... {case.name}")

            orig = run_compile(
                worker, args.original_dir, "_seqc_compiler",
                case, tmpdir, f"{case.name}_orig",
            )

            if args.original_only:
                if orig.error:
                    print(f"  {FAIL}  {case.name}: {orig.error[:120]}")
                    failed += 1
                else:
                    print(f"  {PASS}  {case.name} "
                          f"({len(orig.elf_bytes)} bytes)")
                    passed += 1
                continue

            recon = run_compile(
                worker, args.recon_dir, args.recon_module,
                case, tmpdir, f"{case.name}_recon",
            )

            result = compare_results(case.name, orig, recon)
            print_result(result, verbose=args.verbose)

            if result.passed:
                passed += 1
            else:
                failed += 1

    print()
    total = passed + failed + skipped
    print(f"Results: {passed}/{total} passed, {failed} failed, {skipped} skipped")
    return 0 if failed == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
