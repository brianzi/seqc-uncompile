#!/usr/bin/env python3
"""Fast differential test harness for SeqC compiler reconstruction.

Drop-in replacement for diff_test.py that uses fork-per-test batch workers
for dramatically reduced startup overhead. Each side (original + recon)
loads the .so once, then forks a child per test case with Linux COW
isolation.

Results stream in as they complete — comparison happens immediately when
both sides finish a test. Byte-identical results (matched by SHA-256) skip
ELF parsing entirely.

Usage:
    python3 diff_test_fast.py [same options as diff_test.py] [-j N]

See diff_test.py for full option descriptions.
"""

import argparse
import json
import os
import queue
import subprocess
import sys
import tempfile
import threading
import time
from pathlib import Path
from typing import Dict, List, Optional

# Import comparison machinery from the original harness
sys.path.insert(0, str(Path(__file__).parent))
from diff_test import (
    CompareResult, CompileResult, ElfInfo, SectionDiff, TestCase,
    compare_results, load_test_cases,
)

# ── Terminal colors ────────────────────────────────────────────────────────

PASS = "\033[32mPASS\033[0m"
EQUIV = "\033[33mEQUIV\033[0m"
FAIL = "\033[31mFAIL\033[0m"
DIM = "\033[2m"
RESET = "\033[0m"
BOLD = "\033[1m"


# ── Manifest helpers ───────────────────────────────────────────────────────

def resolve_manifest(cases: List[TestCase]) -> List[dict]:
    """Convert TestCase list to JSON-serializable manifest for the batch worker."""
    entries = []
    for i, c in enumerate(cases):
        entry = {
            "name": c.name,
            "_uid": f"{c.name}_{i}",   # unique file/event key, stable across runs
            "code": c.code,
            "devtype": c.devtype,
            "options": c.options,
            "index": c.index,
        }
        if c.samplerate is not None:
            entry["samplerate"] = c.samplerate
        if c.sequencer is not None:
            entry["sequencer"] = c.sequencer
        entries.append(entry)
    return entries


# ── Streaming reader ───────────────────────────────────────────────────────

def _reader_thread(proc: subprocess.Popen, side: str, q: queue.Queue):
    """Read JSON lines from proc.stdout and push (side, event) to queue."""
    try:
        for line in proc.stdout:
            line = line.strip()
            if not line:
                continue
            try:
                event = json.loads(line)
                event["_side"] = side
                q.put(event)
            except json.JSONDecodeError:
                pass  # ignore non-JSON output
    finally:
        q.put({"_side": side, "event": "_eof"})


# ── Result reading ─────────────────────────────────────────────────────────

def _read_compile_result(output_dir: str, uid: str) -> CompileResult:
    """Read a single compile result from the output directory."""
    elf_path = os.path.join(output_dir, f"{uid}.elf")
    meta_path = os.path.join(output_dir, f"{uid}.json")

    elf_bytes = b""
    if os.path.exists(elf_path):
        with open(elf_path, "rb") as f:
            elf_bytes = f.read()

    meta = {}
    error = None
    if os.path.exists(meta_path):
        with open(meta_path) as f:
            meta = json.load(f)
        error = meta.get("error")
    else:
        error = "no result (worker may have crashed)"

    return CompileResult(elf_bytes, meta, error=error)


# ── Pretty printing ───────────────────────────────────────────────────────

def _fmt_time(seconds: float) -> str:
    if seconds < 1.0:
        return f"{seconds*1000:.0f}ms"
    return f"{seconds:.1f}s"


def _print_result_line(index: int, total: int, name: str, status: str,
                       detail: str, orig_time: Optional[float],
                       recon_time: Optional[float]):
    """Print one result line with counter, status, and timing."""
    counter = f"{DIM}[{index:>{len(str(total))}}/{total}]{RESET}"
    timing = ""
    if orig_time is not None and recon_time is not None:
        timing = f" {DIM}({_fmt_time(orig_time)} + {_fmt_time(recon_time)}){RESET}"
    elif orig_time is not None:
        timing = f" {DIM}({_fmt_time(orig_time)}){RESET}"

    print(f"  {counter}  {status}  {name}{detail}{timing}")


def _print_equiv_details(oe: dict, re: dict, result: CompareResult):
    """Print details for EQUIV results — what differs between the two."""
    indent = "              "
    # ELF difference
    orig_size = oe.get("elf_size", 0)
    recon_size = re.get("elf_size", 0)
    if oe.get("elf_sha256") != re.get("elf_sha256"):
        if orig_size != recon_size:
            print(f"{indent}{DIM}elf differs "
                  f"(orig {orig_size} bytes, recon {recon_size} bytes){RESET}")
        else:
            print(f"{indent}{DIM}elf differs ({orig_size} bytes both, "
                  f"layout/headers differ){RESET}")
    # Meta hash
    if oe.get("meta_sha256") != re.get("meta_sha256"):
        print(f"{indent}{DIM}meta differs{RESET}")
    # Header field differences from compare_results (e.g. e_entry)
    for note in result.notes:
        # Skip generic situation descriptions, only show field diffs
        if ":" in note and ("orig=" in note or "recon=" in note):
            print(f"{indent}{DIM}{note}{RESET}")


def _print_failure_details(result: CompareResult):
    """Print indented details for a failed test."""
    indent = "              "
    if result.orig_error:
        print(f"{indent}{DIM}orig error:{RESET} {result.orig_error[:200]}")
    if result.recon_error:
        print(f"{indent}{DIM}recon error:{RESET} {result.recon_error[:200]}")
    for note in result.notes:
        print(f"{indent}{DIM}{note}{RESET}")
    for sd in result.section_diffs:
        print(f"{indent}{DIM}section '{sd.section}':{RESET} "
              f"{sd.kind} — {sd.detail}")


# ── Main ───────────────────────────────────────────────────────────────────

def main():
    p = argparse.ArgumentParser(
        description="Fast SeqC differential ELF test harness (fork-per-test)")
    p.add_argument("--cases-dir", type=Path,
                   default=Path(__file__).parent / "cases",
                   help="Directory containing manifest.json and .seqc files")
    p.add_argument("--original-dir", type=Path, default=None,
                   help="Directory containing the original _seqc_compiler.so")
    p.add_argument("--recon-dir", type=Path, default=None,
                   help="Directory containing the reconstructed _seqc_compiler.so")
    p.add_argument("--recon-module", default="_seqc_compiler",
                   help="Module name for the reconstruction")
    p.add_argument("--original-only", action="store_true",
                   help="Only run the original compiler (smoke test)")
    p.add_argument("-v", "--verbose", action="store_true")
    p.add_argument("--filter", default=None,
                   help="Only run test cases whose name contains this string")
    p.add_argument("-j", "--jobs", type=int, default=os.cpu_count(),
                   help="Parallelism per side (default: cpu_count)")
    
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
    worker = Path(__file__).resolve().parent / "compile_worker_batch.py"

    if args.original_dir is None:
        args.original_dir = repo_root
    if args.recon_dir is None:
        args.recon_dir = repo_root / "reconstructed" / "build"

    # Parse filter arguments
    tags = args.tags.split(',') if args.tags else None
    exclude_tags = args.exclude_tags.split(',') if args.exclude_tags else None
    groups = args.groups.split(',') if args.groups else None
    exclude_groups = args.exclude_groups.split(',') if args.exclude_groups else None

    # Load and filter test cases
    cases = load_test_cases(args.cases_dir, tags, exclude_tags, groups, exclude_groups)
    if args.filter:
        cases = [c for c in cases if args.filter in c.name]

    if not cases:
        print("No test cases found.")
        return 1

    total = len(cases)
    manifest_json = json.dumps(resolve_manifest(cases))

    print(f"Running {total} test case(s) with -j {args.jobs}")
    print(f"  Original: {args.original_dir}")
    if not args.original_only:
        print(f"  Recon:    {args.recon_dir}")
    print()

    wall_t0 = time.monotonic()

    with tempfile.TemporaryDirectory(prefix="seqc_fast_") as tmpdir:
        orig_dir = os.path.join(tmpdir, "orig")
        recon_dir = os.path.join(tmpdir, "recon")

        if args.original_only:
            return _run_original_only(
                worker, args, manifest_json, orig_dir, total, wall_t0)

        return _run_differential(
            worker, args, manifest_json, orig_dir, recon_dir, total, wall_t0)


def _spawn_worker(worker: Path, module_dir: Path, module_name: str,
                  output_dir: str, jobs: int, manifest_json: str
                  ) -> subprocess.Popen:
    """Launch a batch worker subprocess, pipe manifest to stdin, return Popen."""
    cmd = [
        sys.executable, str(worker),
        "--module-dir", str(module_dir),
        "--module-name", module_name,
        "--output-dir", output_dir,
        "-j", str(jobs),
    ]
    proc = subprocess.Popen(
        cmd, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
        stderr=subprocess.PIPE, text=True,
    )
    # Write manifest and close stdin so the worker can start immediately
    proc.stdin.write(manifest_json)
    proc.stdin.close()
    return proc


def _run_original_only(worker, args, manifest_json, orig_dir, total, wall_t0):
    """Run only the original side with streaming output."""
    proc = _spawn_worker(worker, args.original_dir, "_seqc_compiler",
                         orig_dir, args.jobs, manifest_json)

    q = queue.Queue()
    t = threading.Thread(target=_reader_thread, args=(proc, "orig", q),
                         daemon=True)
    t.start()

    passed = failed = 0
    index = 0
    cum_time = 0.0

    while True:
        event = q.get()
        if event["event"] == "_eof":
            break
        if event["event"] != "done":
            continue

        index += 1
        name = event["name"]
        elapsed = event.get("elapsed_s", 0)
        cum_time += elapsed
        error = event.get("error")
        elf_size = event.get("elf_size", 0)

        if error:
            _print_result_line(index, total, name, FAIL,
                               f": {error[:120]}", elapsed, None)
            failed += 1
        else:
            _print_result_line(index, total, name, PASS,
                               f" ({elf_size} bytes)", elapsed, None)
            passed += 1

    proc.wait()
    wall_time = time.monotonic() - wall_t0

    print()
    print(f"Results: {passed}/{passed+failed} passed, {failed} failed")
    print(f"{DIM}Wall: {_fmt_time(wall_time)} | "
          f"Cumulative compile: {_fmt_time(cum_time)}{RESET}")
    return 0 if failed == 0 else 1


def _run_differential(worker, args, manifest_json, orig_dir, recon_dir,
                      total, wall_t0):
    """Run both sides with streaming comparison."""
    orig_proc = _spawn_worker(worker, args.original_dir, "_seqc_compiler",
                              orig_dir, args.jobs, manifest_json)
    recon_proc = _spawn_worker(worker, args.recon_dir, args.recon_module,
                               recon_dir, args.jobs, manifest_json)

    q = queue.Queue()
    orig_thread = threading.Thread(
        target=_reader_thread, args=(orig_proc, "orig", q), daemon=True)
    recon_thread = threading.Thread(
        target=_reader_thread, args=(recon_proc, "recon", q), daemon=True)
    orig_thread.start()
    recon_thread.start()

    # Collect events per uid, per side; also track uid->name for display
    orig_events: Dict[str, dict] = {}
    recon_events: Dict[str, dict] = {}
    uid_to_name: Dict[str, str] = {}
    compared = set()  # uids already compared

    passed = equiv = failed = 0
    index = 0
    orig_cum = 0.0
    recon_cum = 0.0
    eof_count = 0

    while eof_count < 2:
        event = q.get()

        if event["event"] == "_eof":
            eof_count += 1
            continue
        if event["event"] != "done":
            continue

        side = event["_side"]
        name = event["name"]
        uid = event.get("_uid", name)
        uid_to_name[uid] = name

        if side == "orig":
            orig_events[uid] = event
            orig_cum += event.get("elapsed_s", 0)
        else:
            recon_events[uid] = event
            recon_cum += event.get("elapsed_s", 0)

        # Check if both sides are done for this test
        if uid in compared:
            continue
        if uid not in orig_events or uid not in recon_events:
            continue

        compared.add(uid)
        index += 1

        oe = orig_events[uid]
        re = recon_events[uid]
        ot = oe.get("elapsed_s", 0)
        rt = re.get("elapsed_s", 0)

        # Fast path: identical outcome without reading files
        # Case 1: both succeeded and ELF hashes match
        if (oe.get("error") is None and re.get("error") is None
                and oe.get("elf_sha256") and re.get("elf_sha256")
                and oe["elf_sha256"] == re["elf_sha256"]):
            _print_result_line(index, total, name, PASS, "", ot, rt)
            passed += 1
            continue

        # Case 2: both errored with identical error message
        if (oe.get("error") and re.get("error")
                and oe["error"] == re["error"]):
            _print_result_line(index, total, name, PASS, " (error)", ot, rt)
            passed += 1
            continue

        # Need full comparison — read ELF files
        orig_result = _read_compile_result(orig_dir, uid)
        recon_result = _read_compile_result(recon_dir, uid)
        result = compare_results(name, orig_result, recon_result)

        if result.passed:
            # Sections match but raw ELF bytes differ → EQUIV
            _print_result_line(index, total, name, EQUIV, "", ot, rt)
            _print_equiv_details(oe, re, result)
            equiv += 1
        else:
            detail = ""
            if result.orig_error or result.recon_error:
                detail = " (error)"
            _print_result_line(index, total, name, FAIL, detail, ot, rt)
            _print_failure_details(result)
            failed += 1

    # Wait for subprocesses
    orig_proc.wait()
    recon_proc.wait()

    # Check for any tests that never completed on one side
    all_uids = set(orig_events) | set(recon_events)
    for uid in all_uids:
        if uid in compared:
            continue
        index += 1
        name = uid_to_name.get(uid, uid)
        if uid not in orig_events:
            _print_result_line(index, total, name, FAIL,
                               " (original side never completed)", None, None)
            failed += 1
        elif uid not in recon_events:
            _print_result_line(index, total, name, FAIL,
                               " (recon side never completed)", None, None)
            failed += 1
        compared.add(uid)

    # Check for import failures
    if orig_proc.returncode == 2:
        stderr = orig_proc.stderr.read() if orig_proc.stderr else ""
        print(f"\n{BOLD}ERROR:{RESET} original batch worker import failed: "
              f"{stderr}", file=sys.stderr)
    if recon_proc.returncode == 2:
        stderr = recon_proc.stderr.read() if recon_proc.stderr else ""
        print(f"\n{BOLD}ERROR:{RESET} recon batch worker import failed: "
              f"{stderr}", file=sys.stderr)

    wall_time = time.monotonic() - wall_t0

    print()
    result_total = passed + equiv + failed
    print(f"Results: {passed} passed, {equiv} equiv, {failed} failed"
          f"  ({result_total} total)")
    print(f"{DIM}Wall: {_fmt_time(wall_time)} | "
          f"Orig cumulative: {_fmt_time(orig_cum)} | "
          f"Recon cumulative: {_fmt_time(recon_cum)}{RESET}")
    return 0 if failed == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
