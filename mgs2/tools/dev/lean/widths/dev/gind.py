#!/usr/bin/env python3
"""gind.py <unit> <reg> (dev): after phase 1 on <unit>, the address-taken functions whose summary says register <reg> may be W at their
return whatever their entry (G): the ones that put <reg> into "W generated" after a call through a register; per function the callees
that themselves generate it, else the own lines that write <reg> with a W class"""
import sys, os, time
sys.path.insert(0, '/home/claude/lean/w2/tools')
import wpo2 as W
u = sys.argv[1]; reg = int(sys.argv[2])
t0 = time.time(); W.load_all([u], t0); W.phase1(t0)
gen = [f for f, F in W.FUNCS.items() if F.ind and W.SUMS.get(f, W.BOT)[0] >> reg & 1]
allg = {f for f in W.FUNCS if W.SUMS.get(f, W.BOT)[0] >> reg & 1}
print('ind functions generating W in R%d: %d (all functions: %d)' % (reg, len(gen), len(allg)))
def own(F):
    out = []
    for ev, falls, li in F.blocks:
        for kind, d, c in ev:
            if kind == 'd' and d[0] == reg and d[1][0] not in ('S', 'SP', 'MV', 'P', 'HL', 'REL'): out.append((li, d[1]))
            if kind == 'c' and d[0] in allg: out.append((li, 'call ' + d[0]))
            if kind == 'c' and d[0] == 'JALR': out.append((li, 'JALR'))
    return out
for f in gen[:25]:
    print('  %s: %s' % (f, own(W.FUNCS[f])[:6]))
