#!/usr/bin/env python3
"""eind_src.py <unit> <reg> (dev): after phase 1 on <unit>, the calls through a register at which register <reg> may hold a W value (the
sites that put <reg> into E_IND), and for each the function's entry W set"""
import sys, os, time
sys.path.insert(0, '/home/claude/lean/w2/tools')
import wpo2 as W
import wpd as P
u = sys.argv[1]; reg = int(sys.argv[2])
t0 = time.time(); W.load_all([u], t0); W.phase1(t0)
for name in W.WIDE if hasattr(W, 'WIDE') else []: pass
for name in W.FUNCS: P.WIDE[name] = frozenset()
n = 0
for name, F in W.FUNCS.items():
    if not F.jalr: continue
    # the forward W pass, recording the state at each JALR
    e0 = W.E.get(name, 0) | (W.G_['E_IND'] if F.ind else 0)
    N = F.nb; din = [None] * N; din[0] = (e0, 0, frozenset()); work = [0]; inq = {0}; hits = set()
    while work:
        i = work.pop(); inq.discard(i); v, sp, sl = din[i]; ev, falls, li = F.blocks[i]
        for kind, d, c in ev:
            if kind == 'c' and d[0] == 'JALR' and v >> reg & 1: hits.add(li)
            if kind == 'd':
                nn, cls, deps = d[:3]; t = cls[0]
                if t == 'REL': continue
                w = 0 if t in ('S', 'SP') else (v >> cls[1] & 1 if t == 'MV' else (1 if (t == 'P' and v & cls[1]) else (v >> W.HLR[cls[1]] & 1 if t == 'HL' else (0 if t == 'P' else 1))))
                if t == 'LDS':
                    if sp is None: w = 1
                    else:
                        key = sp + cls[1]; w = 1 if (F.taken and key not in F.savek) else (0 if key in sl else 1)
                if t == 'SP' and nn == 29:
                    if c: sp = None; sl = frozenset()
                    elif sp is not None: sp += cls[1]
                if nn == 29 and t != 'SP': sp = None; sl = frozenset()
                v = (v | (w << nn)) if c else ((v & ~(1 << nn)) | (w << nn))
            elif kind == 'hw':
                bits_, val = d; w = (v >> val[1] & 1) if val[0] == 'R' else (1 if val[0] == 'W' else 0)
                for b_, r in W.HLR.items():
                    if bits_ & b_: v = (v | (w << r)) if c else ((v & ~(1 << r)) | (w << r))
            elif kind == 'st':
                k, val = d
                if sp is None: continue
                key = sp + k; w = (v >> val[1] & 1) if val[0] in ('R', 'L') else 1
                if w: sl = sl - {key}
                elif not c: sl = sl | {key}
            elif kind == 'c':
                nm = d[0]
                if nm in ('BREAK', 'COP0OP', 'COP0W'): continue
                nv = W.capply(v, W.callsum(nm, d[2], d[3])); v = (v | nv) if c else nv
            elif kind == 'g':
                if d >= 0: W.merge(din, d, (v, sp, sl), work, inq)
        if falls and i + 1 < N: W.merge(din, i + 1, (v, sp, sl), work, inq)
    if hits:
        n += 1
        print('%s%s lines %s; entry W %s' % (name, ' (ind)' if F.ind else '', sorted(hits)[:6], W.rregs(e0)[:12]))
print('%d functions' % n)
