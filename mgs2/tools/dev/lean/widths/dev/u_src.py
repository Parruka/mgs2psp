#!/usr/bin/env python3
"""u_src.py <unit> <reg> [function ...] (dev): after wpd.py's fixpoint on <unit>, the functions whose own code (not a callee's summary)
needs entry register <reg> in full, with the labelled source; with function names, the chain of callees that brings the demand to them"""
import sys, os, time, collections
sys.path.insert(0, '/home/claude/lean/w2/tools')
import wpd as P
W = P.W
u = sys.argv[1]; reg = int(sys.argv[2]); names = sys.argv[3:]
t0 = time.time(); P.run([u], t0)
own = []
for f, F in W.FUNCS.items():
    T, U = P.SUM.get(f, P.BOT)
    if not (U >> reg & 1): continue
    P.LAB[0] = {}; r = P.dback(F, P.FW.get(f) or P.wfwd(F)); lab = P.LAB[0]; P.LAB[0] = None
    # the labelled sources reaching the entry register reg
    srcs = [t for k, (b, t) in lab.items() if (r.U >> reg & 1)]
    callee = [t for t in srcs if ' needs R[' in t]
    if any(' needs R[' not in t for t in srcs): own.append(f)
print('functions with U bit %d: %d' % (reg, sum(1 for f in W.FUNCS if P.SUM.get(f, P.BOT)[1] >> reg & 1)))
print('own sources (sample):', own[:30])
for f in (names or own[:6]):
    F = W.FUNCS[f]; P.LAB[0] = {}; P.dback(F, P.FW.get(f) or P.wfwd(F)); lab = P.LAB[0]; P.LAB[0] = None
    print('==', f, 'ind' if F.ind else '', sorted(set(t for k, (b, t) in lab.items()))[:12])
