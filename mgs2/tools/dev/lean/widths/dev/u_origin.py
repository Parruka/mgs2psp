#!/usr/bin/env python3
"""u_origin.py <unit> <reg> [...]: after wpd.py's fixpoint on <unit>, the functions whose summary needs entry register <reg> in full by
their own code (the bit stays with every callee summary's U and T for that register taken out), with the labelled sources"""
import sys, os, time, re
sys.path.insert(0, '/home/claude/lean/w2/tools')
import wpd as P
W = P.W
u = sys.argv[1]; regs = [int(x) for x in sys.argv[2:]]
t0 = time.time(); P.run([u], t0)
SAVE = dict(P.SUM); SI = P.SUMIND[0]
for reg in regs:
    m = ~(1 << reg)
    def strip(s): return (tuple(x & m for x in s[0]), s[1] & m)
    for k in SAVE: P.SUM[k] = strip(SAVE[k])
    P.SUMIND[0] = strip(SI); P._SIF[0] = None
    orig = []
    for f, F in W.FUNCS.items():
        if not (SAVE.get(f, P.BOT)[1] >> reg & 1): continue
        r = P.dback(F, P.FW.get(f) or P.wfwd(F))
        if r.U >> reg & 1: orig.append(f)
    print('R%d: %d functions need it in full, %d by their own code' % (reg, sum(1 for f in SAVE if SAVE[f][1] >> reg & 1), len(orig)))
    for f in orig[:int(os.environ.get('N', '15'))]:
        F = W.FUNCS[f]; P.LAB[0] = {}; r = P.dback(F, P.FW.get(f) or P.wfwd(F)); lab = {b: t for k, (b, t) in P.LAB[0].items()}; P.LAB[0] = None
        print('   %s%s: %s' % (f, ' (ind)' if F.ind else '', [lab.get(b, '?') for b in P.bits(r.D0[reg] >> 33 << 33)][:6]))
    for k in SAVE: P.SUM[k] = SAVE[k]
    P.SUMIND[0] = SI; P._SIF[0] = None
