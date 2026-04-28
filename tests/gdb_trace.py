#!/usr/bin/env python3
"""GDB trace helper — compiles a SeqC snippet through the original binary."""
import sys
sys.path.insert(0, '.')
import _seqc_compiler as sc

code = """
resetOscPhase();
"""

result = sc.compile_seqc(code, 'HDAWG8', {}, 0, samplerate=2.4e9)
print("OK, ELF size:", len(result[0]))
