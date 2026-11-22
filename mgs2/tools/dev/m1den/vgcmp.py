#!/usr/bin/env python3
"""vgcmp.py <A log> <B log>: the pkt: hashes per vblank compared, and the m1vg: runs (vu_vgrp runs in the window) compared in order:
the first run whose VF-before hash differs, the first whose pc differs, and the fallbacks per vblank and pc."""
import re, sys, collections
def load(fn):
    pk, vg, vd = {}, [], {}
    for l in open(fn, errors='replace'):
        if l.startswith('pkt: vb '): p = l.split(); pk[int(p[2])] = p[3]
        elif l.startswith('m1vg: '):
            p = l.split(); vg.append((int(p[1]), int(p[3]), p[5], int(p[7]), p[9]))   # seq, vb, pc, fb, h
        elif l.startswith('m1vgd: '):
            p = l.split(); vd[int(p[1])] = (p[3:35], p[36:68])
    return pk, vg, vd
A, B = load(sys.argv[1]), load(sys.argv[2])
pa, pb = A[0], B[0]; com = sorted(set(pa) & set(pb)); df = [v for v in com if pa[v] != pb[v]]
print('pkt: %d vblanks in both, %d differ%s' % (len(com), len(df), (' (first %d, last %d)' % (df[0], df[-1])) if df else ''))
va, vb_ = A[1], B[1]
print('vgrp runs in the window: A %d, B %d; fallbacks A %d, B %d' % (len(va), len(vb_), sum(r[3] for r in va), sum(r[3] for r in vb_)))
fh = fp = None
for i, (x, y) in enumerate(zip(va, vb_)):
    if fh is None and x[4] != y[4]: fh = i
    if x[2] != y[2]: fp = i; break
for nm, i in (('first VF-before difference', fh), ('first pc difference', fp)):
    if i is None: print(nm + ': none'); continue
    print('%s at run %d: A vb %d pc %s fb %d h %s | B vb %d pc %s fb %d h %s' % ((nm, i) + va[i][1:] + vb_[i][1:]))
fa = collections.Counter((r[1], r[2]) for r in va if r[3]); fb = collections.Counter((r[1], r[2]) for r in vb_ if r[3])
ra = collections.Counter((r[1], r[2]) for r in va); rb = collections.Counter((r[1], r[2]) for r in vb_)
print('fallbacks per (vb, pc) where A and B differ (A fb/runs, B fb/runs):')
for k in sorted(set(fa) | set(fb)):
    if fa[k] != fb[k]: print('  vb %d pc %s: A %d/%d, B %d/%d' % (k[0], k[1], fa[k], ra[k], fb[k], rb[k]))
fd = [(i, x, y) for i, (x, y) in enumerate(zip(va, vb_)) if x[3] != y[3]]
print('first runs whose fallback differs: ' + '; '.join('run %d vb %d pc %s A fb %d B fb %d' % (i, x[1], x[2], x[3], y[3]) for i, x, y in fd[:6]))
