#!/usr/bin/env python3
"""rlz_cmp.py <log a.gz> <log b.gz> <boot binary>: rl_cmp.py for long gzipped logs, streaming: the two runs take the same path, so their
"rl:" lines pair up in order (vb and index must match).  Prints, per logged vblank, the reads and the differing reads, then the differing reads
grouped by reader / writer a / writer b / 256-byte block / lanes (x y z w = 4-byte lanes of the quadword whose bytes differ), with the first
example of each."""
import bisect, gzip, subprocess, sys
syms = []
for l in subprocess.run(['nm', '-n', sys.argv[3]], capture_output=True, text=True).stdout.splitlines():
    p = l.split()
    if len(p) == 3 and p[1] in 'tTwW': syms.append((int(p[0], 16), p[2]))
base = [s for s in syms if s[1] == 'rt_dgt_rd'][0][0]; addrs = [s[0] for s in syms]
def name(o):
    if not (o[0] in 'rw') or o.startswith('wc'): return o
    x = (base + int(o[1:], 16)) & ((1 << 64) - 1)
    i = bisect.bisect_right(addrs, x) - 1
    return '%s+%x' % (syms[i][1], x - syms[i][0]) if i >= 0 else o
def rl(fn):
    for l in gzip.open(fn, 'rt', errors='replace'):
        if l.startswith('rl: vb '): yield l.split()
grp = {}; per = {}; n = 0
for pa, pb in zip(rl(sys.argv[1]), rl(sys.argv[2])):
    if pa[2:4] != pb[2:4]: print('out of step at', pa[2:4], pb[2:4]); break
    vb = int(pa[2]); t = per.setdefault(vb, [0, 0]); t[0] += 1
    if pa[4] == pb[4] and pa[5] == pb[5]: continue
    t[1] += 1
    ad = int(pa[4], 16); va, vbb = pa[5], pb[5]; nb = len(va) // 2
    lanes = set()
    for i in range(nb):
        if va[2 * (nb - 1 - i):2 * (nb - i)] != vbb[2 * (nb - 1 - i):2 * (nb - i)]: lanes.add('xyzw'[((ad + i) >> 2) & 3])
    k = (name(pa[6]), name(pa[7]), name(pb[7]), ad & ~0xff, ''.join(c for c in 'xyzw' if c in lanes), pa[4] != pb[4])
    g = grp.setdefault(k, [0, None, set()]); g[0] += 1; g[2].add(vb)
    if g[1] is None: g[1] = 'vb %d #%s %s: %s / %s%s' % (vb, pa[3], pa[4], va, vbb, '' if pa[4] == pb[4] else ' ADDRESS ' + pb[4])
print('per vblank (reads, differing):', ' '.join('%d:%d/%d' % (v, per[v][0], per[v][1]) for v in sorted(per)))
for k, g in sorted(grp.items(), key=lambda kv: -kv[1][0]):
    print('%6d  block %08x  lanes %-4s reader %s%s\n        writer a %s / b %s\n        vblanks %s\n        first: %s' % (
        g[0], k[3], k[4], k[0], '  ADDRESS DIFFERS' if k[5] else '', k[1], k[2], ' '.join(str(v) for v in sorted(g[2])), g[1]))
