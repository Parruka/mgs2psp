#!/usr/bin/env python3
"""ftop.py ELF FILE FRAMES [N]  (gl35; run with python3 -I): one PspProf window's counts per function of the ELF, a picture, ranked by the
cost model ms = (1.65 x instructions + 68 x I-misses + 86 x D-misses) / 333,000 [C model]."""
import sys, subprocess, bisect, collections
NM = '/home/claude/pspsdk/pspdev/bin/psp-nm'
elf, p, fr = sys.argv[1], sys.argv[2], int(sys.argv[3]); top = int(sys.argv[4]) if len(sys.argv) > 4 else 60
S = []
for l in subprocess.run([NM, '-S', '--defined-only', elf], capture_output=True, text=True).stdout.split('\n'):
    f = l.split()
    if len(f) == 4 and f[2] in 'tTwW': S.append((int(f[0], 16), int(f[1], 16), f[3]))
S.sort(); st = [s[0] for s in S]; F = collections.defaultdict(lambda: [0, 0, 0]); Z = {}
for ln in open(p):
    if ln.startswith('#'): continue
    f = ln.split()
    if len(f) < 4: continue
    pc = int(f[0], 16); k = bisect.bisect_right(st, pc) - 1
    n = S[k][2] if k >= 0 and pc < S[k][0] + S[k][1] else "(not in the ELF's functions)"
    if n[0] != '(': Z[n] = S[k][1]
    x = F[n]; x[0] += int(f[1]); x[1] += int(f[2]); x[2] += int(f[3])
ms = lambda x: (1.65 * x[0] + 68 * x[1] + 86 * x[2]) / 333000.0
t = [sum(F[n][j] for n in F) / fr for j in range(3)]
print('%s: %.0f instructions, %.0f I-misses, %.0f D-misses = %.2f ms a picture [C model]' % (p, t[0], t[1], t[2], ms(t)))
for n, d in sorted(F.items(), key=lambda x: -ms(x[1]))[:top]:
    d = [v / fr for v in d]
    print('   %-30s %7.3f ms  (%9.0f %7.0f %7.0f)  size %s' % (n[:30], ms(d), d[0], d[1], d[2], Z.get(n, '-')))
