import sys; sys.path.insert(0, '.')
import _seqc_compiler as sc
result = sc.compile_seqc('''
wave w = zeros(64);
playWave(w);
''', 'HDAWG8', {}, 0, samplerate=2.4e9)
