#!/usr/bin/env python3
"""ind_sum.py [unit ...] (dev): after wpd.py's fixpoint, the address-taken functions' summaries: which of them put each bit into the
union a call through a register uses (U: entry registers needed in full; T: entry registers reaching a return register other than
itself), and the labelled sources of their U bits"""
import sys, os, time, collections
sys.path.insert(0, '/home/claude/lean/w2/tools')
import wpd as P
W = P.W
units = sys.argv[1:] or W.D.UNITS
t0 = time.time(); P.run(units, t0)
ub = collections.defaultdict(list); tb = collections.defaultdict(list)
for f, F in W.FUNCS.items():
    if not F.ind: continue
    T, U = P.SUM.get(f, P.BOT)
    for r in P.bits(U): ub[r].append(f)
    for r in range(1, 32):
        for s in P.bits(T[r]):
            if s != r: tb[(r, s)].append(f)
print('U_IND: per entry register, the functions needing it in full')
for r in sorted(ub): print('  R%d: %d: %s' % (r, len(ub[r]), ub[r][:12]))
print('T_IND: (return register <- entry register): functions')
for k in sorted(tb): print('  R%d <- R%d: %d: %s' % (k[0], k[1], len(tb[k]), tb[k][:8]))
