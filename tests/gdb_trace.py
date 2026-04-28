import sys; sys.path.insert(0, '.')
import _seqc_compiler as sc

code = open('tests/cases/hdawg_prefetch.seqc').read()
result = sc.compile_seqc(code, 'HDAWG8', '', 0, samplerate=2.4e9)
print(f"ELF size: {len(result[0])}")
