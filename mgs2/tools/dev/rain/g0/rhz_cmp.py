#!/usr/bin/env python3
"""rhcmp.py <a.log.gz> <b.log.gz> [<c.log.gz> <d.log.gz>]: per-vblank read hashes (rh: lines) of two trace runs compared;
with a second pair, the sets of differing vblanks compared (task #165, H3)"""
import sys, gzip
def load(fn):
    d = {}
    for l in gzip.open(fn, 'rt', errors='replace'):
        if l.startswith('rh: vb '):
            p = l.split(); d[int(p[2])] = (p[3], p[4])
    return d
def ranges(vs):
    out = []; s = p = None
    for v in vs:
        if s is None: s = p = v
        elif v == p + 1: p = v
        else: out.append((s, p)); s = p = v
    if s is not None: out.append((s, p))
    return out
def diff(fa, fb):
    a, b = load(fa), load(fb); vs = sorted(set(a) & set(b))
    dc = [v for v in vs if a[v][1] != b[v][1]]; dh = [v for v in vs if a[v] != b[v]]
    print('%s vs %s: %d vblanks in both (%d / %d), counts differ %d, bytes differ %d' % (fa, fb, len(vs), len(a), len(b), len(dc), len(dh)))
    rs = ranges(dh); print('  ranges (%d):' % len(rs), ' '.join(('%d' % s) if s == e else '%d-%d(%d)' % (s, e, e - s + 1) for s, e in rs)[:1500])
    return set(dh)
x = diff(sys.argv[1], sys.argv[2])
if len(sys.argv) > 4:
    y = diff(sys.argv[3], sys.argv[4])
    print('first pair only (%d):' % len(x - y), ' '.join('%d' % v for v in sorted(x - y))[:1500])
    print('second pair only (%d):' % len(y - x), ' '.join('%d' % v for v in sorted(y - x))[:1500])
    print('both', len(x & y))
