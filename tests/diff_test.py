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
    e_shoff = hdr[5]
    e_shentsize = hdr[10]
    e_shnum = hdr[11]
    e_shstrndx = hdr[12]

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
    wavepath: Optional[str] = None
    waveforms: Optional[str] = None
    filename: Optional[str] = None


def load_test_cases(cases_dir: Path, tags=None, exclude_tags=None, groups=None, exclude_groups=None, manifest_file=None) -> List[TestCase]:
    """Load test cases from a manifest file with optional filtering."""
    if manifest_file:
        manifest = Path(manifest_file)
        if not manifest.is_absolute():
            manifest = cases_dir / manifest
    else:
        manifest = cases_dir / "manifest.json"
    
    if not manifest.exists():
        print(f"ERROR: {manifest} not found", file=sys.stderr)
        sys.exit(2)

    # Use manifest_loader for v1/v2 support
    try:
        from manifest_loader import load_manifest
        test_cases_v2 = load_manifest(manifest)
    except Exception as e:
        print(f"ERROR loading manifest: {e}", file=sys.stderr)
        sys.exit(2)

    # Apply tag/group filtering
    filtered = test_cases_v2
    if tags:
        tag_set = set(tags)
        filtered = [t for t in filtered if tag_set & set(t.tags)]
    if exclude_tags:
        exclude_tag_set = set(exclude_tags)
        filtered = [t for t in filtered if not (exclude_tag_set & set(t.tags))]
    if groups:
        group_set = set(groups)
        filtered = [t for t in filtered if any(g in group_set for g in t.groups)]
    if exclude_groups:
        exclude_group_set = set(exclude_groups)
        filtered = [t for t in filtered if not any(g in exclude_group_set for g in t.groups)]

    # Convert to old TestCase format for compatibility
    cases = []
    def resolve_case_path(value: Optional[str]) -> Optional[str]:
        if value is None:
            return None
        if value == "" or value == "@none":
            return value
        path = Path(value)
        if path.is_absolute():
            return value
        return str(cases_dir / path)

    def resolve_case_path_list(value: Optional[str]) -> Optional[str]:
        if value is None:
            return None
        if value == "@none":
            return value
        parts = [resolve_case_path(part) for part in value.split(";")]
        return ";".join(part for part in parts if part)

    for tc in filtered:
        # If "file" key is present, read code from that file
        code = tc.code or ""
        if tc.file:
            code_path = cases_dir / tc.file
            if code_path.exists():
                code = f"@{code_path}"
            else:
                print(f"WARNING: {code_path} not found, skipping {tc.name}")
                continue

        cases.append(TestCase(
            name=tc.name,
            code=code,
            devtype=tc.devtype,
            options=tc.options,
            index=tc.index,
            samplerate=tc.samplerate,
            sequencer=tc.sequencer,
            wavepath=resolve_case_path(tc.wavepath) if tc.wavepath is not None else None,
            waveforms=resolve_case_path_list(tc.waveforms),
            filename=tc.filename,
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
    if case.wavepath is not None:
        cmd += ["--wavepath", case.wavepath]
    if case.waveforms is not None:
        cmd += ["--waveforms", case.waveforms]
    if case.filename is not None:
        cmd += ["--filename", case.filename]

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
    equiv: bool = False       # True when sections match but raw bytes differ
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
        # Both errored — treat as pass (error message text may differ due to
        # non-deterministic UB in the original binary for invalid inputs;
        # what matters is that both sides reject the input).
        result.notes.append(f"both errored")
        result.orig_error = orig.error
        result.recon_error = recon.error
        result.passed = True
        if orig.error != recon.error:
            result.notes.append("error messages differ (accepted)")
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

        # SHT_NOBITS (8) sections occupy no file space — `data` bytes read
        # at sh_offset are meaningless (they belong to whatever PROGBITS
        # section is colocated). Compare type/size/addr instead. This is
        # the same semantics ELFIO/loaders use for BSS-style sections.
        SHT_NOBITS = 8
        if so.type == SHT_NOBITS or sr.type == SHT_NOBITS:
            if so.type != sr.type:
                result.section_diffs.append(SectionDiff(
                    sname, "type",
                    f"orig={so.type} recon={sr.type}"))
            elif so.size != sr.size:
                result.section_diffs.append(SectionDiff(
                    sname, "size",
                    f"orig={so.size} recon={sr.size} (NOBITS)"))
            elif so.addr != sr.addr:
                result.section_diffs.append(SectionDiff(
                    sname, "addr",
                    f"orig=0x{so.addr:x} recon=0x{sr.addr:x} (NOBITS)"))
            continue

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
    if result.passed and not result.byte_identical:
        result.equiv = True
    return result


# ── Reporting ──────────────────────────────────────────────────────────────

PASS = "\033[32mPASS\033[0m"
EQUIV = "\033[33mEQUIV\033[0m"
FAIL = "\033[31mFAIL\033[0m"
SKIP = "\033[33mSKIP\033[0m"


def print_result(r: CompareResult, verbose: bool = False):
    if r.byte_identical:
        status = PASS
        extra = " (byte-identical)"
    elif r.equiv:
        status = EQUIV
        extra = ""
    elif r.passed:
        status = PASS
        extra = ""
    else:
        status = FAIL
        extra = " (error)" if (r.orig_error or r.recon_error) else ""

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
                   help="Directory containing test case files")
    p.add_argument("--manifest", type=str, default=None,
                   help="Manifest file to load (default: manifest.json in cases-dir)")
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
    
    # v2.0 manifest filtering
    p.add_argument("--tags", default=None,
                   help="Only run tests with these tags (comma-separated)")
    p.add_argument("--exclude-tags", default=None,
                   help="Exclude tests with these tags (comma-separated)")
    p.add_argument("--groups", default=None,
                   help="Only run tests from these groups (comma-separated)")
    p.add_argument("--exclude-groups", default=None,
                   help="Exclude tests from these groups (comma-separated)")
    
    # Discovery
    p.add_argument("--list-groups", action="store_true",
                   help="List all groups and exit")
    p.add_argument("--list-tags", action="store_true",
                   help="List all tags and exit")
    p.add_argument("--show-only", action="store_true",
                   help="Show selected tests in pretty format (don't run)")
    
    args = p.parse_args()
    
    # Handle discovery commands
    if args.list_groups or args.list_tags:
        from manifest_loader import load_manifest
        from collections import Counter
        if args.manifest:
            manifest = Path(args.manifest)
            if not manifest.is_absolute():
                manifest = args.cases_dir / manifest
        else:
            manifest = args.cases_dir / "manifest.json"
        tests = load_manifest(manifest)
        
        if args.list_groups:
            groups = Counter(t.groups[0] if t.groups else '(none)' for t in tests)
            print("Groups:")
            for group, count in sorted(groups.items()):
                print(f"  {group:30} {count:4} tests")
            return 0
        
        if args.list_tags:
            all_tags = Counter()
            for t in tests:
                all_tags.update(t.tags)
            print("Tags:")
            for tag, count in sorted(all_tags.items()):
                print(f"  {tag:30} {count:4} tests")
            return 0
    
    # Handle show-only (delegate to show_manifest.py)
    if args.show_only:
        import subprocess
        cmd = [sys.executable, str(Path(__file__).parent / "show_manifest.py")]
        if args.cases_dir:
            cmd.extend(["--cases-dir", str(args.cases_dir)])
        if args.manifest:
            cmd.extend(["--manifest", args.manifest])
        if args.filter:
            cmd.extend(["--filter", args.filter])
        if args.tags:
            cmd.extend(["--tags", args.tags])
        if args.exclude_tags:
            cmd.extend(["--exclude-tags", args.exclude_tags])
        if args.groups:
            cmd.extend(["--groups", args.groups])
        if args.exclude_groups:
            cmd.extend(["--exclude-groups", args.exclude_groups])
        if args.verbose:
            cmd.append("-v")
        return subprocess.call(cmd)

    # Resolve paths
    repo_root = Path(__file__).resolve().parent.parent
    worker = Path(__file__).resolve().parent / "compile_worker.py"

    if args.original_dir is None:
        args.original_dir = repo_root
    if args.recon_dir is None:
        args.recon_dir = repo_root / "reconstructed" / "build"

    # Parse filter arguments
    tags = args.tags.split(',') if args.tags else None
    exclude_tags = args.exclude_tags.split(',') if args.exclude_tags else None
    groups = args.groups.split(',') if args.groups else None
    exclude_groups = args.exclude_groups.split(',') if args.exclude_groups else None

    # Load test cases
    cases = load_test_cases(args.cases_dir, tags, exclude_tags, groups, exclude_groups, args.manifest)
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
    equiv = 0
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
                if result.equiv:
                    equiv += 1
                else:
                    passed += 1
            else:
                failed += 1

    print()
    total = passed + equiv + failed + skipped
    print(f"Results: {passed} passed, {equiv} equiv, {failed} failed, {skipped} skipped"
          f"  ({total} total)")
    return 0 if failed == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
