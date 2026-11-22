#!/usr/bin/env python3
"""rh_cmp.py <log a> <log b>: compares the "rh: vb N hash reads" lines of two x86 GEN_DG_TRACE runs with MGS2_READ_HASH=1 (boot.c
dgt_rh_add: every guest CPU read outside the draw pipeline, its address and the word read).  Two runs that differ only in the renderer
(e.g. MGS2_OPT=ffffffbf full / ffffffff reduced) read the same values in a vblank iff its line is equal; prints the vblanks that differ
(as ranges) and whether the read counts differ too (a different count: the game took another path)."""
import sys
def load(fn):
    d = {}
    for l in open(fn, errors='replace'):
        if l.startswith('rh: vb '):
            p = l.split(); d[int(p[2])] = (p[3], int(p[4]))
    return d
a = load(sys.argv[1]); b = load(sys.argv[2])
vs = sorted(set(a) & set(b))
print('vblanks in both: %d (a only %d, b only %d)' % (len(vs), len(set(a) - set(b)), len(set(b) - set(a))))
bad = [v for v in vs if a[v] != b[v]]
cnt = [v for v in bad if a[v][1] != b[v][1]]
print('differ: %d vblanks, of them %d with a different read count' % (len(bad), len(cnt)))
rs = []
for v in bad:
    if rs and v <= rs[-1][1] + 1: rs[-1][1] = v
    else: rs.append([v, v])
for s, e in rs[:60]:
    c = sum(1 for v in range(s, e + 1) if v in a and v in b and a[v][1] != b[v][1])
    print('  vb %d-%d (%d)%s' % (s, e, e - s + 1, ', read count differs in %d' % c if c else ''))
if len(rs) > 60: print('  ... %d ranges' % len(rs))
