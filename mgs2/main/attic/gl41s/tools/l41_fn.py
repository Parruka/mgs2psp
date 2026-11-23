#!/usr/bin/env python3
"""l41_fn.py PSPDIR FILE REGEX (gl41s; python3 -I): per-function cost a step [C model] of the functions matching REGEX in one model file."""
import sys, subprocess, bisect, collections, re
NM = '/home/claude/pspsdk/pspdev/bin/psp-nm'
P, fn, rx = sys.argv[1], sys.argv[2], re.compile(sys.argv[3])
S = []
for elf in (P + '/MGS2T.elf', P + '/ovl_w00a.elf'):
    for l in subprocess.run([NM, '-S', '--defined-only', elf], capture_output=True, text=True).stdout.split('\n'):
        f = l.split()
        if len(f) == 4 and f[2] in 'tTwW':
            S.append((int(f[0], 16), int(f[1], 16), f[3]))
S.sort(); st = [s[0] for s in S]
F = collections.defaultdict(lambda: [0, 0, 0])
for ln in open(fn):
    f = ln.split()
    if ln.startswith('#') or len(f) < 4:
        continue
    pc = int(f[0], 16); k = bisect.bisect_right(st, pc) - 1
    n = S[k][2] if k >= 0 and pc < S[k][0] + S[k][1] else '?'
    x = F[n]; x[0] += int(f[1]); x[1] += int(f[2]); x[2] += int(f[3])
for n, x in sorted(F.items(), key=lambda kv: -(1.65 * kv[1][0] + 68 * kv[1][1] + 86 * kv[1][2])):
    if rx.search(n):
        d = [v / 80.0 for v in x]
        print('%8.4f ms  instr %9.0f  I %6.0f  D %6.0f  %s' % ((1.65 * d[0] + 68 * d[1] + 86 * d[2]) / 333000, d[0], d[1], d[2], n))
