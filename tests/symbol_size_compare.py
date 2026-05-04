#!/usr/bin/env python3
"""
symbol_size_compare.py - Compare symbol sizes between original and recon binaries.

Uses `nm -S --defined-only` to extract text-segment symbol sizes from both
.so files, matches on mangled name (exact), demangles for display, and reports
functions with the largest size discrepancies.

Typical usage:
    python tests/symbol_size_compare.py \\
        --orig _seqc_compiler.so \\
        --recon reconstructed/build_release/_seqc_compiler.so

NOTE: The recon binary should be built with Release optimization (clang++ -O2)
to make size comparison meaningful against the original. Comparing a Debug (-O0)
recon to the optimized original will show large ratios everywhere.
"""

import argparse
import subprocess
import sys
from pathlib import Path


def nm_sizes(binary: Path) -> dict[str, int]:
    """Return {mangled_name: byte_size} for all defined text symbols with nonzero size."""
    result = subprocess.run(
        ["nm", "-S", "--defined-only", str(binary)],
        capture_output=True, text=True, check=True
    )
    syms = {}
    for line in result.stdout.splitlines():
        parts = line.split()
        # nm -S output with size: <addr> <size> <type> <name>
        if len(parts) < 4:
            continue
        try:
            size = int(parts[1], 16)
        except ValueError:
            continue
        sym_type = parts[2]
        name = parts[3]
        # Only text symbols (T = global, t = local)
        if sym_type not in ('T', 't'):
            continue
        if size == 0:
            continue
        syms[name] = size
    return syms


def demangle(names: list[str]) -> dict[str, str]:
    """Demangle a list of mangled C++ symbol names via c++filt."""
    if not names:
        return {}
    result = subprocess.run(
        ["c++filt"] + names,
        capture_output=True, text=True, check=True
    )
    demangled = result.stdout.splitlines()
    return dict(zip(names, demangled))


def main():
    parser = argparse.ArgumentParser(
        description="Compare symbol sizes between original and reconstructed binaries."
    )
    parser.add_argument(
        "--orig",
        default="_seqc_compiler.so",
        help="Path to original binary (default: _seqc_compiler.so)",
    )
    parser.add_argument(
        "--recon",
        default="reconstructed/build_release/_seqc_compiler.so",
        help="Path to reconstructed binary (default: reconstructed/build_release/_seqc_compiler.so)",
    )
    parser.add_argument(
        "--top",
        type=int,
        default=50,
        help="Number of top symbols to show by abs(delta) (default: 50)",
    )
    parser.add_argument(
        "--min-orig-bytes",
        type=int,
        default=32,
        help="Minimum original symbol size to include (default: 32, filters tiny functions)",
    )
    parser.add_argument(
        "--sort",
        choices=["delta", "ratio"],
        default="delta",
        help="Sort by absolute delta (default) or ratio",
    )
    args = parser.parse_args()

    orig_path = Path(args.orig)
    recon_path = Path(args.recon)

    for p in (orig_path, recon_path):
        if not p.exists():
            print(f"ERROR: binary not found: {p}", file=sys.stderr)
            sys.exit(1)

    print(f"Loading symbols from original: {orig_path}", file=sys.stderr)
    orig_syms = nm_sizes(orig_path)
    print(f"  {len(orig_syms)} text symbols with nonzero size", file=sys.stderr)

    print(f"Loading symbols from recon:    {recon_path}", file=sys.stderr)
    recon_syms = nm_sizes(recon_path)
    print(f"  {len(recon_syms)} text symbols with nonzero size", file=sys.stderr)

    # Intersection on mangled names
    common = set(orig_syms.keys()) & set(recon_syms.keys())
    print(f"Matched symbols (mangled):     {len(common)}", file=sys.stderr)

    # Build comparison rows, filter by min size
    rows = []
    for name in common:
        orig_sz = orig_syms[name]
        if orig_sz < args.min_orig_bytes:
            continue
        recon_sz = recon_syms[name]
        delta = recon_sz - orig_sz
        ratio = recon_sz / orig_sz
        rows.append((name, orig_sz, recon_sz, delta, ratio))

    print(f"After min-size filter ({args.min_orig_bytes}B): {len(rows)} symbols", file=sys.stderr)

    # Sort
    if args.sort == "ratio":
        rows.sort(key=lambda r: abs(r[4] - 1.0), reverse=True)
    else:
        rows.sort(key=lambda r: abs(r[3]), reverse=True)

    top_rows = rows[: args.top]

    # Demangle for display
    mangled_names = [r[0] for r in top_rows]
    dm = demangle(mangled_names)

    # Stats
    all_ratios = [r[4] for r in rows]
    larger = sum(1 for r in rows if r[4] > 2.0)
    smaller = sum(1 for r in rows if r[4] < 0.5)
    median_ratio = sorted(all_ratios)[len(all_ratios) // 2] if all_ratios else 0.0

    # --- Output ---
    print()
    print(f"Symbol size comparison: {orig_path.name}  vs  {recon_path.name}")
    print(f"Sorted by: abs({'delta' if args.sort == 'delta' else 'ratio deviation'}), "
          f"top {args.top} of {len(rows)} matched symbols (min orig size: {args.min_orig_bytes}B)")
    print()

    # Column widths
    hdr = f"{'Rank':>4}  {'Delta':>7}  {'Ratio':>6}  {'Orig':>7}  {'Recon':>7}  Symbol"
    print(hdr)
    print("-" * min(120, len(hdr) + 60))

    for rank, (name, orig_sz, recon_sz, delta, ratio) in enumerate(top_rows, 1):
        demangled = dm.get(name, name)
        # Truncate demangled name for display
        if len(demangled) > 80:
            demangled = demangled[:77] + "..."
        delta_str = f"{delta:+d}"
        ratio_str = f"{ratio:.2f}x"
        print(f"{rank:>4}  {delta_str:>7}  {ratio_str:>6}  {orig_sz:>7}  {recon_sz:>7}  {demangled}")

    print()
    print("─" * 60)
    print(f"Total matched symbols (after size filter): {len(rows)}")
    print(f"Median ratio (recon/orig):                 {median_ratio:.2f}x")
    print(f"Symbols >2x larger in recon:               {larger}")
    print(f"Symbols <0.5x smaller in recon:            {smaller}")

    # Symbols present in recon but not original (unique to recon)
    recon_only = set(recon_syms.keys()) - set(orig_syms.keys())
    orig_only = set(orig_syms.keys()) - set(recon_syms.keys())
    print(f"Symbols in recon only (not in orig):       {len(recon_only)}")
    print(f"Symbols in orig only (not in recon):       {len(orig_only)}")


if __name__ == "__main__":
    main()
