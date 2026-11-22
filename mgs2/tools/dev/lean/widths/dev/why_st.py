#!/usr/bin/env python3
"""why_st.py <state> <function:register> ... (dev): from wpd.py's saved fixpoint (WPD_STATE), why each named local is 64-bit, without
the whole run: (1) its cut points with the demands that reach them (unconditional ones labelled by source, return registers), the
calls after which a caller demands those return registers (WHY_DEPTH levels, WHY_MAX calls each); (2) for an entry cut, where the W
value at entry comes from: address-taken (E_IND) and the direct callers whose W set at the call has the register."""
import sys, os
sys.path.insert(0, '/home/claude/lean/w2/tools')
import wpstate as S
st = S.load(sys.argv[1]); P = S.P; W = S.W
DEPTH = int(os.environ.get('WHY_DEPTH', '2')); MAXC = int(os.environ.get('WHY_MAX', '4'))
_cache = {}
def labelled(f):
    if f not in _cache:
        P.LAB[0] = {}
        F = W.FUNCS[f]; r = P.dback(F, P.wfwd(F))
        _cache[f] = (r, {b: t for k, (b, t) in P.LAB[0].items()})
        P.LAB[0] = None
    return _cache[f]
def desc(m, lab, act):
    return [lab.get(b, '?%d' % b) for b in P.bits(m >> 33 << 33)], [b for b in P.bits(m & ((1 << 32) - 1)) if act >> b & 1]
def callers_of(f):
    out = list(W.CG['callers'].get(f, ()))
    if W.FUNCS[f].ind: out += ['<calls through a register: %d sites>' % len(W.CG['jalrs'])]
    return out
def why_ret(f, r, depth, ind):
    if depth <= 0: return
    k = 0
    for g in W.CG['callers'].get(f, ()):
        rg, lab = labelled(g); act = P.dret(g)
        for key, (nm, aft) in rg.sites.items():
            if nm != f: continue
            a = aft.get(r, 0)
            if not (a >> 32 or a & act): continue
            u, rr = desc(a, lab, act)
            print('%s%s calls %s (line %d): R%d after it needed by %s%s' % (ind, g, nm, W.FUNCS[g].blocks[key[0]][2], r, u[:3],
                  ', and by %s\'s return registers %s' % (g, rr) if rr else ''))
            for r2 in rr[:2]: why_ret(g, r2, depth - 1, ind + '    ')
            k += 1
            if k >= MAXC: print(ind + '...'); return
    if W.FUNCS[f].ind and DIND_HAS(r): print('%s(and every call through a register: DIND has R%d)' % (ind, r))
def DIND_HAS(r): return P.DIND[0] >> r & 1
def w_at_calls(g, f):
    """the W set at each direct call of f in g (wfwd's transfer, recorded at the call events)"""
    F = W.FUNCS[g]; N = F.nb; out = []
    e0 = W.E.get(g, 0) | (W.G_['E_IND'] if F.ind else 0)
    if not N: return out
    din = [None] * N; din[0] = (e0, 0, frozenset()); work = [0]; inq = {0}; seen = {}
    while work:
        i = work.pop(); inq.discard(i)
        v, sp, sl = din[i]; ev, falls, _ = F.blocks[i]
        for j, (kind, d, c) in enumerate(ev):
            if kind == 'c' and d[0] == f: seen[(i, j)] = seen.get((i, j), 0) | v
            if kind == 'd':
                n, cls, deps = d[:3]; t = cls[0]
                if t == 'REL': continue
                if t == 'S': w = 0
                elif t == 'MV': w = v >> cls[1] & 1
                elif t == 'P': w = 1 if v & cls[1] else 0
                elif t == 'HL': w = v >> W.HLR[cls[1]] & 1
                elif t == 'LDS':
                    if sp is None: w = 1
                    else:
                        key = sp + cls[1]; w = 1 if (F.taken and key not in F.savek) else (0 if key in sl else 1)
                elif t == 'SP': w = 0
                else: w = 1
                if t == 'SP' and n == 29:
                    if c: sp = None; sl = frozenset()
                    elif sp is not None: sp += cls[1]
                if n == 29 and t != 'SP': sp = None; sl = frozenset()
                v = (v | (w << n)) if c else ((v & ~(1 << n)) | (w << n))
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
                nm, rin, rout, mout = d
                if nm in ('BREAK', 'COP0OP', 'COP0W'): continue
                nv = W.capply(v, W.callsum(nm, rout, mout)); v = (v | nv) if c else nv
            elif kind == 'g':
                if d >= 0: W.merge(din, d, (v, sp, sl), work, inq)
        if falls and i + 1 < N: W.merge(din, i + 1, (v, sp, sl), work, inq)
    return [(F.blocks[i][2], v) for (i, j), v in sorted(seen.items())]
for t in sys.argv[2:]:
    f, n = t.split(':'); n = int(n); F = W.FUNCS[f]
    wide = n in P.WIDE.get(f, ()); act = P.dret(f)
    print('== %s r%d: 64-bit %s; address-taken %s; W at entry %s; return demand %s' % (f, n, wide, F.ind, W.regs(W.E.get(f, 0)),
          W.rregs(act)))
    if wide:     # explain the widening: this local 32-bit again, the rest as at the fixpoint
        F0 = F; W0 = P.WIDE[f]; P.WIDE[f] = frozenset(W0 - {n})
        dict.__setitem__(W.FUNCS, f, S.func(st, f, set(P.WIDE[f]))); _cache.pop(f, None)
    r, lab = labelled(f)
    if wide: dict.__setitem__(W.FUNCS, f, F0); P.WIDE[f] = W0; _cache.pop(f, None)
    for key, (nn, m, li) in r.cuts.items():
        if nn != n: continue
        u, rr = desc(m, lab, act)
        print('  cut at %s: unconditional %s; return registers %s' % (li, u[:4], rr))
        for r2 in rr[:3]: why_ret(f, r2, DEPTH, '    ')
        if li == 'entry':
            if F.ind and W.G_['E_IND'] >> n & 1: print('    W at entry from calls through a register (E_IND has r%d)' % n)
            k = 0
            for g in W.CG['callers'].get(f, ()):
                for line, v in w_at_calls(g, f):
                    if v >> n & 1:
                        print('    W in r%d at %s\'s call (line %d)' % (n, g, line)); k += 1
                if k >= MAXC: print('    ...'); break
