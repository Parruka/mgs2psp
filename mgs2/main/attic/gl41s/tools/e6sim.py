#!/usr/bin/env python3
"""e6sim.py LOG A B [R] (gl41s, investigation only): simulates E6b, the suffix-state cache, on a trace twin log's 'e6tr H|M hash lo hi
base slot' lines between 'gl41 levers vb A' and 'gl41 levers vb B'.  E6b keeps the FNV-1a state of VUPROG at one word index H (>= every
dirty range's end since the last flush) for the images seen since then: when the words [H, 1024) are unwritten since an entry was made,
FNV-1a(VUPROG) = the entry's final hash iff the state at H equals the entry's state (FNV-1a's step is a bijection of the state for a
fixed word), so a rehash costs H words.  Flush: a dirty end above H (H then = that end rounded up to 16), an unknown write (base -1),
and the decay: every R rehashes, H = the period's highest end if that is lower (R 0: no decay).  Prints hits, words hashed a rehash."""
import sys
p, a, b = sys.argv[1], int(sys.argv[2]), int(sys.argv[3]); R = int(sys.argv[4]) if len(sys.argv) > 4 else 0
seq = []; on = False
for l in open(p, errors='replace'):
    if l.startswith('gl41 levers vb '):
        vb = int(l.split()[3].rstrip(':')); on = a <= vb < b
    elif on and l.startswith('e6tr '):
        f = l.split(); seq.append((f[2], int(f[3]), int(f[4]), int(f[5])))
up = lambda x: min(1024, (x + 15) // 16 * 16)
H = 0; ent = set(); hit = miss = fl = 0; words = 0; pmax = 0; k = 0; lost = 0; big = 0
for h, lo, hi, base in seq:
    if base < 0:
        ent.clear(); fl += 1; lost += 1
    if hi > H:
        H = up(hi); ent.clear(); fl += 1; big += 1
    pmax = max(pmax, hi); k += 1
    if h in ent:
        hit += 1; words += H
    else:
        miss += 1; words += 1024; ent.add(h)
    if R and k % R == 0:
        if up(pmax) < H:
            H = up(pmax); ent.clear(); fl += 1
        pmax = 0
n = len(seq)
print(f'vb {a}-{b} R {R}: {n} rehashes ({n / (b - a):.2f} a vblank); hits {hit} ({hit / max(n, 1):.1%}), misses {miss}; flushes {fl} '
      f'(unknown writes {lost}, end above H {big}); final H {H}; words hashed a rehash {words / max(n, 1):.0f} of 1024 '
      f'(FNV work saved {1 - words / max(n, 1) / 1024:.1%})')
