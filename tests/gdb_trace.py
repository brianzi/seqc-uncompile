#!/usr/bin/env python3
"""Script to be run under gdb to trace binary execution."""
import sys
sys.path.insert(0, '.')
import _seqc_compiler as sc

# This will trigger the code path we want to trace
elf, extra = sc.compile_seqc(
    'wave w = zeros(64);\nplayWave(w);',
    'HDAWG8', '', 0, samplerate=2.4e9)
print(f"ELF size: {len(elf)}")
print(f"Messages: {extra.get('messages', '')}")
