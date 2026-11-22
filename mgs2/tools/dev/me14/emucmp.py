# emucmp.py <emu log> <reference emu log> <x86 log>: task #149 step 2 on the PSP build: the hash and vuh lines against a reference PSP
# build's emulator log (the game must not change), the frame digest lines ("picture: vb N buffer ...") of the emulator log against the
# x86 run's lines of the same vblanks, the list words and the memory lines.
import sys, re
def load(p):
    hs, vu, fr, other = [], [], {}, []
    for l in open(p, errors='replace'):
        l = l.rstrip('\n')
        if l.startswith('hash: vb '): hs.append(l)
        elif l.startswith('vuh'): vu.append(l)
        elif l.startswith('picture: vb ') and ' buffer ' in l: fr[int(l.split()[2])] = l.split(' ', 3)[3]
        elif re.match(r'(picture: \d+ frames made|free memory|memory:|memory at|picture heap|translator: the picture code|picture: on:)', l): other.append(l)
    return hs, vu, fr, other
E, R, X = load(sys.argv[1]), load(sys.argv[2]), load(sys.argv[3])
print('hash lines: %d, reference %d, equal %s' % (len(E[0]), len(R[0]), E[0] == R[0]))
print('vuh lines: %d, reference %d, equal %s' % (len(E[1]), len(R[1]), E[1] == R[1]))
bad = [v for v in sorted(E[2]) if X[2].get(v) != E[2][v]]
print('frame digest lines: %d (vb %s..%s), x86 has them all: %s, differ: %d%s' % (len(E[2]), min(E[2]) if E[2] else '-', max(E[2]) if E[2] else '-',
      all(v in X[2] for v in E[2]), len(bad), (' first ' + str(bad[:5])) if bad else ''))
for l in E[3]: print('  ' + l[:330])
print('reference:')
for l in R[3]: print('  ' + l[:330])
