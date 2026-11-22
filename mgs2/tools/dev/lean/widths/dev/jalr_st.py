#!/usr/bin/env python3
"""jalr_st.py <state> [register] (dev): jalr_why.py from wpd.py's saved fixpoint: the calls through a register after which the caller
needs a register's full value (what makes DIND), per site the registers and the labelled sources of each unconditional demand (or
the caller's return registers); with [register], only the sites that need that one."""
import sys, os, collections
sys.path.insert(0, '/home/claude/lean/w2/tools')
import wpstate as S
st = S.load(sys.argv[1]); P = S.P; W = S.W
only = int(sys.argv[2]) if len(sys.argv) > 2 else None
cnt = collections.Counter(); rows = []
for f in sorted(set(W.CG['jalrs'])):
    F = W.FUNCS[f]; P.LAB[0] = {}
    r = P.dback(F, P.wfwd(F)); lab = {b: t for k, (b, t) in P.LAB[0].items()}; P.LAB[0] = None; act = P.dret(f)
    for key, (nm, aft) in r.sites.items():
        if nm != 'JALR': continue
        m = {x: a for x, a in aft.items() if (a >> 32) or (a & act)}
        if not m or (only is not None and only not in m): continue
        for x in m: cnt[x] += 1
        out = []
        for x, a in sorted(m.items()):
            if only is not None and x != only: continue
            u = [lab.get(b, '?') for b in P.bits(a >> 33 << 33)]
            rr = [b for b in P.bits(a & ((1 << 32) - 1)) if act >> b & 1]
            out.append('R%d: %s%s' % (x, u[:2], (' ret %s' % rr) if rr else ''))
        rows.append('%s line %d: %s' % (f, F.blocks[key[0]][2], '; '.join(out)))
print('functions with calls through a register: %d; sites with demand after the call: %d; per register %s' %
      (len(set(W.CG['jalrs'])), len(rows), sorted(cnt.items())))
for l in rows[:int(os.environ.get('N', '80'))]: print(l)
