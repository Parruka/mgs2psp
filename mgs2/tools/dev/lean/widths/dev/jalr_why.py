#!/usr/bin/env python3
"""jalr_why.py [unit ...] (dev): after wpd.py's fixpoint, the calls through a register after which the caller needs a register's
full value: per site the registers, and the labelled sources of each unconditional demand (or the caller's return registers)"""
import sys, os, time, collections
sys.path.insert(0, '/home/claude/lean/w2/tools')
import wpd as P
W = P.W
units = sys.argv[1:] or W.D.UNITS
t0 = time.time(); P.run(units, t0)
cnt = collections.Counter(); rows = []
for f, r in P.RES.items():
    act = P.dret(f)
    for key, (nm, aft) in r.sites.items():
        if nm != 'JALR': continue
        m = {x: a for x, a in aft.items() if (a >> 32) or (a & act)}
        if m: rows.append((f, key, m))
for f, key, m in rows:
    for x in m: cnt[x] += 1
print('JALR sites with demand after the call: %d; per register %s' % (len(rows), sorted(cnt.items())))
P.LAB[0] = {}
for f, key, m in rows[:int(os.environ.get('N', '60'))]:
    F = W.FUNCS[f]; P.LAB[0] = {}
    r = P.dback(F, P.FW.get(f) or P.wfwd(F)); lab = {b: t for k, (b, t) in P.LAB[0].items()}; act = P.dret(f)
    nm, aft = r.sites[key]
    out = []
    for x, a in sorted(aft.items()):
        if not ((a >> 32) or (a & act)): continue
        u = [lab.get(b, '?') for b in P.bits(a >> 33 << 33)]
        rr = [b for b in P.bits(a & ((1 << 32) - 1)) if act >> b & 1]
        out.append('R%d: %s%s' % (x, u[:2], (' ret %s' % rr) if rr else ''))
    print('%s line %d: %s' % (f, F.blocks[key[0]][2], '; '.join(out)))
P.LAB[0] = None
