#!/usr/bin/env python3
"""Script to be run under gdb to trace binary execution for hdawg_playWave_multi."""
import sys
sys.path.insert(0, '.')
import _seqc_compiler as sc

elf, extra = sc.compile_seqc(
    'wave w1 = zeros(64);\nwave w2 = ones(64);\nplayWave(w1);\nplayWave(w2);',
    'HDAWG8', '', 0, samplerate=2.4e9)
print(f"ELF size: {len(elf)}")
print(f"Messages: {extra.get('messages', '')}")
