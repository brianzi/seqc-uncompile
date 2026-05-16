#!/usr/bin/env python3
"""Batch compile worker using fork-per-test for subprocess isolation.

Loads a _seqc_compiler module once, then forks a child process for each
test case. Each child compiles one test and writes results to the output
directory. Children are forked from a process that already has the .so
loaded, so they benefit from Linux COW semantics — near-zero startup cost
per test while maintaining full memory isolation.

Progress is reported as JSON lines on stdout (one per completed test).

Interface:
    python3 compile_worker_batch.py --module-dir DIR --module-name NAME \\
        --output-dir DIR [-j N]
    < [JSON array of test cases on stdin]

Each test case dict has keys: name, code, devtype, options, index,
samplerate (optional), sequencer (optional), and any additional
compile_seqc kwargs such as wavepath, waveforms, or filename.
The "code" field is either inline source or "@/absolute/path.seqc".

Output per test:
    <output-dir>/<name>.elf   — raw ELF bytes (empty on error)
    <output-dir>/<name>.json  — metadata dict or {"error": "..."}

Stdout protocol (JSON lines):
    {"event":"done","name":"...","exit_code":0,"signal":null,
     "elapsed_s":0.03,"elf_sha256":"...","meta_sha256":"...",
     "elf_size":1284,"error":null}

Exit codes:
    0 = all children completed (individual failures in .json files)
    2 = import/setup error
"""

import argparse
import hashlib
import importlib
import json
import os
import signal
import sys
import time
import traceback


def compile_one(mod, case: dict, output_dir: str):
    """Run in forked child. Compile one test case, write results, _exit."""
    # Use _uid (assigned by the harness) as the file key so that two test
    # cases with the same display name don't clobber each other's files.
    uid = case.get("_uid", case["name"])
    elf_path = os.path.join(output_dir, f"{uid}.elf")
    meta_path = os.path.join(output_dir, f"{uid}.json")

    # Timeout: 60 seconds
    signal.alarm(3)

    # Resolve code
    code = case["code"]
    if code.startswith("@"):
        with open(code[1:], "r") as f:
            code = f.read()

    # Build kwargs
    kwargs = {}
    if case.get("samplerate") is not None:
        kwargs["samplerate"] = case["samplerate"]
    if case.get("sequencer") is not None:
        kwargs["sequencer"] = case["sequencer"]
    for key in ("wavepath", "waveforms", "filename"):
        if case.get(key) is not None:
            val = case[key]
            # Manifest sentinel: "@none" means "pass Python None explicitly"
            if val == "@none":
                val = None
            kwargs[key] = val

    try:
        result = mod.compile_seqc(code, case["devtype"], case.get("options", ""), case.get("index", 0), **kwargs)
    except Exception as exc:
        _write_meta(
            meta_path,
            {
                "error": str(exc),
                "traceback": traceback.format_exc(),
            },
        )
        _write_bytes(elf_path, b"")
        os._exit(1)

    # Unpack result
    elf_bytes, meta = result[0], result[1]
    if isinstance(meta, str):
        try:
            meta = json.loads(meta)
        except json.JSONDecodeError:
            meta = {"raw_meta": meta}
    elif not isinstance(meta, dict):
        meta = {"raw_meta": str(meta)}

    _write_bytes(elf_path, elf_bytes)
    _write_meta(meta_path, meta)
    os._exit(0)


def _write_bytes(path, data):
    with open(path, "wb") as f:
        f.write(data)


def _write_meta(path, data):
    with open(path, "w") as f:
        json.dump(data, f, indent=2, default=str)


def _sha256_file(path):
    """Return hex digest of file contents, or None if file doesn't exist."""
    try:
        h = hashlib.sha256()
        with open(path, "rb") as f:
            for chunk in iter(lambda: f.read(65536), b""):
                h.update(chunk)
        return h.hexdigest()
    except FileNotFoundError:
        return None


def _emit(event: dict):
    """Write a JSON line to stdout and flush."""
    sys.stdout.write(json.dumps(event, default=str) + "\n")
    sys.stdout.flush()


def main():
    p = argparse.ArgumentParser(description="Batch SeqC compile worker (fork-per-test)")
    p.add_argument("--module-dir", required=True, help="Directory containing the _seqc_compiler .so")
    p.add_argument("--module-name", default="_seqc_compiler", help="Module name to import")
    p.add_argument("--output-dir", required=True, help="Directory to write result files")
    p.add_argument(
        "-j", "--jobs", type=int, default=os.cpu_count(), help="Max concurrent forked children (default: cpu_count)"
    )
    args = p.parse_args()

    # Read manifest from stdin
    manifest = json.load(sys.stdin)
    if not manifest:
        return 0

    os.makedirs(args.output_dir, exist_ok=True)

    # Import module
    sys.path.insert(0, args.module_dir)
    try:
        old_flags = sys.getdlopenflags()
        sys.setdlopenflags(old_flags | 0x1)  # RTLD_LAZY
        try:
            mod = importlib.import_module(args.module_name)
        finally:
            sys.setdlopenflags(old_flags)
    except Exception as exc:
        print(f"import failed: {exc}", file=sys.stderr)
        return 2

    # Fork per test with concurrency limit
    # active: pid -> {"name": str, "t0": float}
    active = {}
    max_jobs = max(1, args.jobs)

    for case in manifest:
        # If at capacity, wait for one child
        while len(active) >= max_jobs:
            _reap_one(active, args.output_dir)

        t0 = time.monotonic()
        pid = os.fork()
        if pid == 0:
            # Child — close stdout so our writes don't leak into the
            # parent's JSON line stream
            try:
                os.close(1)
                os.open(os.devnull, os.O_WRONLY)  # fd 1 = /dev/null
                compile_one(mod, case, args.output_dir)
            except Exception:
                os._exit(2)
        else:
            active[pid] = {"name": case["name"], "_uid": case.get("_uid", case["name"]), "t0": t0}

    # Reap remaining children
    while active:
        _reap_one(active, args.output_dir)

    return 0


def _reap_one(active: dict, output_dir: str):
    """Wait for one child, emit a 'done' event, handle abnormal termination."""
    pid, status = os.waitpid(-1, 0)
    info = active.pop(pid, None)
    if info is None:
        return

    name = info["name"]
    uid = info.get("_uid", name)
    elapsed = time.monotonic() - info["t0"]

    elf_path = os.path.join(output_dir, f"{uid}.elf")
    meta_path = os.path.join(output_dir, f"{uid}.json")

    sig = None
    exit_code = None

    if os.WIFSIGNALED(status):
        sig = os.WTERMSIG(status)
        # Child didn't get to write files
        if not os.path.exists(meta_path):
            _write_meta(meta_path, {"error": f"killed by signal {sig}"})
        if not os.path.exists(elf_path):
            _write_bytes(elf_path, b"")
    elif os.WIFEXITED(status):
        exit_code = os.WEXITSTATUS(status)

    # Read error from meta if present
    error = None
    if os.path.exists(meta_path):
        try:
            with open(meta_path) as f:
                meta = json.load(f)
            error = meta.get("error")
        except Exception:
            error = "failed to read meta JSON"

    # Compute hashes and size
    elf_sha = _sha256_file(elf_path)
    meta_sha = _sha256_file(meta_path)
    elf_size = 0
    try:
        elf_size = os.path.getsize(elf_path)
    except OSError:
        pass

    _emit(
        {
            "event": "done",
            "name": name,
            "_uid": uid,
            "exit_code": exit_code,
            "signal": sig,
            "elapsed_s": round(elapsed, 4),
            "elf_sha256": elf_sha,
            "meta_sha256": meta_sha,
            "elf_size": elf_size,
            "error": error,
        }
    )


if __name__ == "__main__":
    sys.exit(main())
