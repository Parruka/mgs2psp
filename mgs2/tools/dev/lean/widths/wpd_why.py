#!/usr/bin/env python3
"""wpd_why.py <function:register> ... [unit ...] (dev): after wpd.py's fixpoint, why each named local is wide: at each of its cut points
the full uses whose demand reaches it (each unconditional demand labelled by its source) and the return registers whose demand reaches
it; then for each such return register the calls (direct, through a register) after which a caller demands it, recursively
(WHY_DEPTH levels, WHY_MAX calls per level)."""
import sys, os, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import wpd as P
W = P.W; D = P.D
DEPTH = int(os.environ.get('WHY_DEPTH', '3')); MAXC = int(os.environ.get('WHY_MAX', '4'))
_cache = {}
def labelled(f):
    if f not in _cache:
        P.LAB[0] = {}
        F = W.FUNCS[f]; fw = P.FW.get(f) or P.wfwd(F)
        r = P.dback(F, fw)
        _cache[f] = (r, {b: t for k, (b, t) in P.LAB[0].items()})
        P.LAB[0] = None
    return _cache[f]
def desc(m, lab, act):
    u = [lab.get(b, '?%d' % b) for b in P.bits(m >> 33 << 33)]
    rr = [b for b in P.bits(m & ((1 << 32) - 1)) if act >> b & 1]
    return u, rr
def callers_of(f):
    F = W.FUNCS[f]; out = list(W.CG['callers'][f])
    if F.ind: out += W.CG['jalrs']
    return out
def why_ret(f, r, depth, ind):
    """the calls after which a caller demands R[r] of f"""
    if depth <= 0: return
    k = 0
    for g in callers_of(f):
        rg, lab = labelled(g); act = P.dret(g)
        for key, (nm, aft) in rg.sites.items():
            if not (nm == f or (nm == 'JALR' and W.FUNCS[f].ind) or (nm == 'JSIF' and f in W.SIF)): continue
            a = aft.get(r, 0)
            if not (a >> 32 or a & act): continue
            u, rr = desc(a, lab, act)
            print('%s%s calls %s (line %d): R[%d] after it needed by %s%s' % (ind, g, nm, W.FUNCS[g].blocks[key[0]][2], r, u[:3],
                  ', and %s\'s return registers %s' % (g, rr) if rr else ''))
            for r2 in rr[:2]: why_ret(g, r2, depth - 1, ind + '    ')
            k += 1
            if k >= MAXC: print(ind + '...'); return
def main():
    args = sys.argv[1:]; tg = [a for a in args if ':' in a]; units = [a for a in args if ':' not in a] or D.UNITS
    t0 = time.time(); P.run(units, t0)
    for t in tg:
        f, n = t.split(':'); n = int(n)
        wide0 = n in P.WIDE[f]; F0 = W.FUNCS[f]
        if wide0:     # explain the widening: the local narrow again, the rest as at the fixpoint
            P.WIDE[f] = P.WIDE[f] - {n}; U = W.CG['US'][F0.unit]; D.ctx(U)
            W.FUNCS[f] = W.parse(f, U, D.emit(U, f, set(P.WIDE[f])), set(P.WIDE[f])); _cache.pop(f, None); P.FW.pop(f, None)
        r, lab = labelled(f); act = P.dret(f)
        if wide0: P.WIDE[f] = P.WIDE[f] | {n}; W.FUNCS[f] = F0; _cache.pop(f, None); P.FW.pop(f, None)
        print('== %s r%d: wide %s; return demand %s' % (f, n, wide0, W.rregs(act)))
        for key, (nn, m, li) in r.cuts.items():
            if nn != n: continue
            u, rr = desc(m, lab, act)
            print('  cut at %s: unconditional %s; return registers %s' % (li, u[:4], rr))
            for r2 in rr[:3]: why_ret(f, r2, DEPTH, '    ')
main()
