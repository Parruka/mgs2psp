#!/usr/bin/env python3
"""wstat.py <wpo json> [units]: the widths found against v27's (a 64-bit construct -> every local int64_t), weighted by the PSP model's
per-function cost in free play (v26's PspProf dump, 100 steps)"""
import sys, json, re, subprocess, bisect, collections
sys.path.insert(0, '/home/claude/lean/w2/tools')
import rcdrv as D
rc = D.rc
NM = '/home/claude/pspsdk/pspdev/bin/psp-nm'
def syms(*elfs):
    S = {}
    for e in elfs:
        for l in subprocess.run([NM, '-n', e], capture_output=True, text=True).stdout.splitlines():
            p = l.split()
            if len(p) == 3 and p[1] in 'tTwW': S[int(p[0], 16)] = p[2]
    S = sorted(S.items()); return [a for a, _ in S], [n for _, n in S]
A, N = syms('/home/claude/v26/d/MGS2T.elf', '/home/claude/v26/d/ovl_w00a.elf'); steps = 100.0
cost = collections.defaultdict(float)
for l in open('/home/claude/v26/model2/mf'):
    if l[0] == '#': continue
    a, c, i, m = l.split(); a = int(a, 16)
    k = bisect.bisect_right(A, a) - 1; n = N[k].split('.')[0] if k >= 0 else '?'
    cost[n] += (1.4 * int(c) + 70 * (int(i) + int(m))) / steps / 333000
J = json.load(open(sys.argv[1])); WR = {k: set(v) for k, v in J['wr'].items()}
units = sys.argv[2:] or ['elf']
tot = collections.defaultdict(float); rows = []
for u in units:
    src = open(D.PN + '/' + D.hname(u)).read()
    for m in rc.FN.finditer(src):
        name, body = m.group(1), m.group(2)
        used = {int(x) for x in re.findall(r'\bR\[(\d+)\]', body)} - {0}
        old = used if D.IS_WIDE(body) else set()
        new = WR.get(name, set()) & used if name in WR else None
        c = cost.get(name, 0.0)
        if new is None: k = 'not analysed'
        elif not old and not new: k = '32-bit, stays'
        elif not old: k = '32-bit, gains 64-bit registers'
        elif not new: k = '64-bit -> all 32-bit'
        elif new == old: k = '64-bit, unchanged'
        else: k = '64-bit -> partly 32-bit'
        tot[k] += c
        if c > 0.05 and old != new: rows.append((c, name, len(old), len(new or ()), sorted(new or ()), J['why'].get(name, {})))
for k, v in sorted(tot.items()): print('%-34s %.2f ms/step' % (k, v))
rows.sort(reverse=True)
for c, name, a, b, nw, why in rows[:40]: print('%.3f %-22s %2d -> %2d %s %s' % (c, name, a, b, nw, why))
