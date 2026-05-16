#!/usr/bin/env python3
"""Subprocess worker that loads a _seqc_compiler module and compiles SeqC code.

Invoked by diff_test.py in a subprocess to isolate module loading (the original
and reconstructed .so files would conflict if loaded in the same process).

Usage:
    python3 compile_worker.py --module-dir DIR --code CODE --devtype DEV \
        --index N --output-elf OUT.elf --output-meta OUT.json \
        [--samplerate SR] [--sequencer SEQ]

The worker writes:
  - The raw ELF bytes to --output-elf
  - A JSON dict with compilation metadata to --output-meta

Exit codes:
  0 = success
  1 = compilation error (error message written to --output-meta as {"error": "..."})
  2 = usage / import error
"""

import argparse
import importlib
import json
import sys
import traceback


def main():
    p = argparse.ArgumentParser(description="SeqC compile worker")
    p.add_argument("--module-dir", required=True,
                   help="Directory containing _seqc_compiler.so")
    p.add_argument("--module-name", default="_seqc_compiler",
                   help="Module name to import (default: _seqc_compiler)")
    p.add_argument("--code", required=True,
                   help="SeqC source code (or @filepath to read from file)")
    p.add_argument("--devtype", required=True)
    p.add_argument("--options", default="")
    p.add_argument("--index", type=int, default=0)
    p.add_argument("--samplerate", type=float, default=None)
    p.add_argument("--sequencer", default=None)
    p.add_argument("--wavepath", default=None)
    p.add_argument("--waveforms", default=None)
    p.add_argument("--filename", default=None)
    p.add_argument("--output-elf", required=True)
    p.add_argument("--output-meta", required=True)
    args = p.parse_args()

    # Resolve source code
    code = args.code
    if code.startswith("@"):
        with open(code[1:], "r") as f:
            code = f.read()

    # Load the module from the specified directory.
    # Use RTLD_LAZY to tolerate undefined symbols from incomplete reconstruction
    # (they'll only crash if actually called at runtime during compilation).
    sys.path.insert(0, args.module_dir)
    try:
        import ctypes
        old_flags = sys.getdlopenflags()
        sys.setdlopenflags(old_flags | 0x1)  # RTLD_LAZY
        try:
            mod = importlib.import_module(args.module_name)
        finally:
            sys.setdlopenflags(old_flags)
    except Exception as exc:
        write_meta(args.output_meta, {"error": f"import failed: {exc}"})
        return 2

    # Build kwargs
    kwargs = {}
    if args.samplerate is not None:
        kwargs["samplerate"] = args.samplerate
    if args.sequencer is not None:
        kwargs["sequencer"] = args.sequencer
    if args.wavepath is not None:
        kwargs["wavepath"] = None if args.wavepath == "@none" else args.wavepath
    if args.waveforms is not None:
        kwargs["waveforms"] = None if args.waveforms == "@none" else args.waveforms
    if args.filename is not None:
        kwargs["filename"] = None if args.filename == "@none" else args.filename

    # Compile
    try:
        result = mod.compile_seqc(
            code, args.devtype, args.options, args.index, **kwargs
        )
    except Exception as exc:
        write_meta(args.output_meta, {
            "error": str(exc),
            "traceback": traceback.format_exc(),
        })
        # Write empty ELF
        with open(args.output_elf, "wb") as f:
            pass
        return 1

    # Unpack result — original returns (bytes, dict), reconstruction may
    # return (bytes, str) where the str is JSON.
    elf_bytes, meta = result[0], result[1]
    if isinstance(meta, str):
        try:
            meta = json.loads(meta)
        except json.JSONDecodeError:
            meta = {"raw_meta": meta}
    elif not isinstance(meta, dict):
        meta = {"raw_meta": str(meta)}

    # Write outputs
    with open(args.output_elf, "wb") as f:
        f.write(elf_bytes)
    write_meta(args.output_meta, meta)
    return 0


def write_meta(path, data):
    with open(path, "w") as f:
        json.dump(data, f, indent=2, default=str)


if __name__ == "__main__":
    sys.exit(main())
