#!/usr/bin/env python3
"""e6tr.py LOG A B (gl41s, investigation only): the E6 trace lines ('e6tr H|M hash lo hi base slot') of a twin log between the
'gl41 levers vb A' and 'gl41 levers vb B' lines: rehashes, distinct images, the dirty range, and caches of N images simulated on the
hash sequence (round robin and LRU; a hit when the image is cached: the range test is not simulated)."""
import sys, collections
p, a, b = sys.argv[1], int(sys.argv[2]), int(sys.argv[3])
seq = []; on = False; vb = 0
for l in open(p, errors='replace'):
    if l.startswith('gl41 levers vb '):
        vb = int(l.split()[3].rstrip(':')); on = a <= vb < b
    elif on and l.startswith('e6tr '):
        f = l.split(); seq.append((f[1], f[2], int(f[3]), int(f[4])))
n = len(seq); vbs = b - a
print(f'vb {a}-{b}: {n} rehashes ({n / vbs:.2f} a vblank), hits {sum(1 for s in seq if s[0] == "H")} (32-image cache with the range test)')
c = collections.Counter(s[1] for s in seq); print(f'distinct images {len(c)}; top: ' + ' '.join(f'{h}x{k}' for h, k in c.most_common(12)))
w = [s[3] - s[2] for s in seq if s[3] > s[2]]
if w: w.sort(); print(f'dirty range words: median {w[len(w) // 2]}, mean {sum(w) / len(w):.0f}, max {w[-1]}, min {w[0]}')
for N in (2, 3, 4, 6, 8, 12, 16):
    rr = []; nx = 0; hr = 0; lru = []; hl = 0
    for s in seq:
        h = s[1]
        if h in rr: hr += 1
        elif len(rr) < N: rr.append(h)
        else: rr[nx] = h; nx = (nx + 1) % N
        if h in lru: hl += 1; lru.remove(h); lru.append(h)
        else:
            lru.append(h)
            if len(lru) > N: lru.pop(0)
    print(f'  N={N:2d}: round robin hits {hr / n:.1%}, LRU hits {hl / n:.1%}')
print('first 40:', ' '.join(s[1][:4] + s[0] for s in seq[:40]))
