#!/usr/bin/env python3
"""u_chain.py <unit> <reg> <function>: after wpd.py's fixpoint, follow entry register <reg>'s unconditional demand from <function> down
the callees until a function's own use"""
import sys, os, time, re
sys.path.insert(0, '/home/claude/lean/w2/tools')
import wpd as P
W = P.W
u = sys.argv[1]; reg = int(sys.argv[2]); f = sys.argv[3]
t0 = time.time(); P.run([u], t0)
seen = set()
while f and f not in seen:
    seen.add(f); F = W.FUNCS.get(f)
    if F is None: print('  (not translated here:', f, ')'); break
    P.LAB[0] = {}; r = P.dback(F, P.FW.get(f) or P.wfwd(F)); lab = P.LAB[0]; P.LAB[0] = None
    # which labels reach entry register reg: re-run per label is costly; list the labels naming R[reg] or r<reg>
    T, U = P.SUM.get(f, P.BOT)
    cand = sorted(set(t for k, (b, t) in lab.items()))
    print('== %s (U has R%d: %s) labels: %s' % (f, reg, bool(U >> reg & 1), cand[:20]))
    nxt = None
    for t in cand:
        m = re.match(r'(\S+) needs R\[%d\]' % reg, t)
        if m: nxt = m.group(1); break
    if nxt is None: break
    f = nxt
