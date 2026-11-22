#!/usr/bin/env python3
"""fncost.py <profile> <calls> <steps> <elf>... -- <names>: per function, self cost (instr, imiss, dmiss, model ms a step) and its callers' share"""
import sys, subprocess, bisect, collections
NM = '/home/claude/pspsdk/pspdev/bin/psp-nm'
a = sys.argv[1:]; i = a.index('--'); tg = a[i + 1:]; a = a[:i]
pf, cf, steps, elfs = a[0], a[1], int(a[2]), a[3:]
S = {}
for e in elfs:
    for l in subprocess.run([NM, '-n', e], capture_output=True, text=True).stdout.splitlines():
        p = l.split()
        if len(p) == 3 and p[1] in 'tTwW': S[int(p[0], 16)] = p[2]
byname = {n: x for x, n in S.items()}
L = sorted(S.items()); A = [x for x, _ in L]; N = [n for _, n in L]
def fn(x):
    k = bisect.bisect_right(A, x) - 1; return N[k] if k >= 0 else '?'
cost = collections.defaultdict(lambda: [0, 0, 0])
for l in open(pf):
    if l.startswith('#'): continue
    p = l.split()
    if len(p) < 4: continue
    try: x = int(p[0], 16); v = [int(p[1]), int(p[2]), int(p[3])]
    except ValueError: continue
    c = cost[fn(x)]
    for k in range(3): c[k] += v[k]
C = collections.defaultdict(collections.Counter)
for l in open(cf):
    p = l.split()
    if len(p) < 3: continue
    try: site, t, c = int(p[0], 16), int(p[1], 16), int(p[2])
    except ValueError: continue
    C[t][fn(site)] += c
def ms(v): return (1.647 * v[0] + 68.2 * v[1] + 85.9 * v[2]) / 333000 / steps
for n in tg:
    v = cost.get(n, [0, 0, 0]); t = byname.get(n); tot = sum(C[t].values()) if t is not None else 0
    print('%-24s instr %10d imiss %7d dmiss %7d  %.3f ms  calls %.1f a step' % (n, v[0], v[1], v[2], ms(v), tot / steps))
    if t is not None:
        for k, c in C[t].most_common(5): print('      from %-24s %8.1f a step' % (k, c / steps))
