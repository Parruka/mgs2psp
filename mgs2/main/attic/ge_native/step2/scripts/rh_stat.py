#!/usr/bin/env python3
"""rh_stat.py <log a> <log b>: per window of the route to vb 37,800, vblanks whose read hash differs and whose read count differs
(the read-hash twin's "rh: vb <v> <hash> <count>" lines); then the strict state hash lines ("hash" checkpoints) compared."""
import sys
def load(fn):
    d = {}
    for l in open(fn, errors='replace'):
        if l.startswith('rh: vb '):
            p = l.split(); d[int(p[2])] = (p[3], int(p[4]))
    return d
a = load(sys.argv[1]); b = load(sys.argv[2])
W = [(0, 4299, 'boot, title'), (4300, 23999, 'opening and its polygon demos'), (24000, 25999, 'deck, first play'), (26000, 28899, 'deck, the talk'),
     (28900, 29299, 'between'), (29300, 31399, 'deck, rain'), (31400, 37799, 'w01a and on')]
print('vblanks in both: %d of %d / %d' % (len(set(a) & set(b)), len(a), len(b)))
for lo, hi, nm in W:
    vs = [v for v in range(lo, hi + 1) if v in a and v in b]
    bad = [v for v in vs if a[v] != b[v]]; cnt = [v for v in bad if a[v][1] != b[v][1]]
    rd = sum(a[v][1] for v in vs); dc = sum(abs(a[v][1] - b[v][1]) for v in cnt)
    ex = ' first %s' % bad[:6] if bad and len(bad) < 50 else ''
    print('  vb %5d-%5d %-30s %5d vblanks, hash differs in %5d, count differs in %5d (sum of count differences %d of %d reads)%s' % (lo, hi, nm, len(vs), len(bad), len(cnt), dc, rd, ex))
