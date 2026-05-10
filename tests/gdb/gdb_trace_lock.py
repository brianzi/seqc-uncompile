#!/usr/bin/env python3
"""Driver for GDB-tracing Prefetch::allocate's high-NodeType dispatch on a
Lock-using SeqC program. Mirrors the source of tests/cases/uhfli_misc_funcs.seqc
without the surrounding noise so the trace output is short."""
import sys
sys.path.insert(0, '.')
import _seqc_compiler as sc

code = """
wave w = zeros(16);
lock(w);
playWave(w);
unlock(w);
"""

result = sc.compile_seqc(code, 'UHFLI', {}, 0)
print("OK, ELF size:", len(result[0]))
