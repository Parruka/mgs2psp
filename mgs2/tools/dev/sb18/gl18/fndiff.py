#!/usr/bin/env python3
"""fndiff.py <elf>[,<ovl elf>...] <dump a> <dump b> <steps> [top]: two PspProf dumps of one build summed per function (the ELFs'
symbol tables) and compared, model ms a step = (1.647 x instructions + 68.2 x I-misses + 85.9 x D-misses) / 333,000 / steps
(renderer_notes §11.25's fit); prints the totals and the functions with the largest differences (b - a)."""
import sys, subprocess, bisect, collections
NM = '/home/claude/pspsdk/pspdev/bin/psp-nm'
elfs, da, db, steps = sys.argv[1].split(','), sys.argv[2], sys.argv[3], int(sys.argv[4]); top = int(sys.argv[5]) if len(sys.argv) > 5 else 25
S = {}
for e in elfs:
    for l in subprocess.run([NM, '-n', e], capture_output=True, text=True).stdout.splitlines():
        p = l.split()
        if len(p) == 3 and p[1] in 'tTwW': S[int(p[0], 16)] = p[2]
S = sorted(S.items()); A = [a for a, _ in S]; N = [n for _, n in S]
def load(f):
    F = collections.defaultdict(lambda: [0, 0, 0])
    for l in open(f):
        if l[0] == '#': continue
        a, i, im, dm = l.split(); a = int(a, 16)
        k = bisect.bisect_right(A, a) - 1; n = N[k] if k >= 0 else '?'
        x = F[n]; x[0] += int(i); x[1] += int(im); x[2] += int(dm)
    return F
def ms(x): return (1.647 * x[0] + 68.2 * x[1] + 85.9 * x[2]) / 333000 / steps
a, b = load(da), load(db)
ta = [sum(x[j] for x in a.values()) for j in range(3)]; tb = [sum(x[j] for x in b.values()) for j in range(3)]
print('a %.2f ms a step, b %.2f (%+.2f): instructions %+.2f M, I-misses %+.1f k, D-misses %+.1f k' % (ms(ta), ms(tb), ms(tb) - ms(ta), (tb[0] - ta[0]) / 1e6, (tb[1] - ta[1]) / 1e3, (tb[2] - ta[2]) / 1e3))
keys = set(a) | set(b); z = [0, 0, 0]
d = sorted(keys, key=lambda k: -abs(ms(b.get(k, z)) - ms(a.get(k, z))))
for k in d[:top]:
    xa, xb = a.get(k, z), b.get(k, z)
    print('%+7.3f ms  (%6.3f -> %6.3f)  instr %+8.3f M  im %+7.1f k  dm %+7.1f k  %s' % (ms(xb) - ms(xa), ms(xa), ms(xb), (xb[0] - xa[0]) / 1e6, (xb[1] - xa[1]) / 1e3, (xb[2] - xa[2]) / 1e3, k))
