#!/usr/bin/env python3
import sys
sys.path.insert(0, '.')
import _seqc_compiler as sc

code = """
wave w_1 = zeros(64);
wave w_2 = zeros(64);
wave w_3 = zeros(64);

assignWaveIndex(1, w_1);
assignWaveIndex(2, w_2);
assignWaveIndex(3, w_3);

while (true) {
    executeTableEntry(1);
    executeTableEntry(2);
    executeTableEntry(3);
}
"""

try:
    result = sc.compile_seqc(code, 'HDAWG8', {}, 0, samplerate=2.4e9)
    print("OK, ELF size:", len(result[0]))
except Exception as e:
    print("Error:", e)
