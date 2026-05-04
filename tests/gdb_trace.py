#!/usr/bin/env python3
import sys
sys.path.insert(0, '.')
import _seqc_compiler as sc

code = """
wave w = placeholder(1000);
playWaveIndexed(w, 200, 128);
"""

result = sc.compile_seqc(code, 'UHFLI', {}, 0)
print("OK, ELF size:", len(result[0]))
