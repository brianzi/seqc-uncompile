import sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, '.')
import _seqc_compiler as sc

# This triggers playZero with a ternary (Var) argument
source = '''var c = getUserReg(0);
playZero((c > 0) ? 32 : 64);
'''
result = sc.compile_seqc(source, 'HDAWG8', {}, 0, samplerate=2.4e9)
print("OK:", len(result) if result else "None")
