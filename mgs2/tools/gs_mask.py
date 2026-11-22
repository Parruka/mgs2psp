#!/usr/bin/env python3
"""gs_mask.py <dg trace report>... > mask.txt: renderer-only memory for boot.c MGS2_GS_MASK (the game-state hash "gs").
Each report (boot.c MGS2_DG_TRACE in a GEN_DG_TRACE build) lists "mask a b" (written inside the draw pipeline from its first vblank on,
never read after such a write by game code) and "keep a b" (written inside, read by game code) word ranges.  The result: what one
route masks and no route keeps, as ascending "mask a b" lines (hex byte offsets in EE RAM; 0..0x3fff is the scratchpad)."""
import sys

def ranges(fn, kind):
    out = []
    for ln in open(fn):
        p = ln.split()
        if len(p) == 3 and p[0] == kind:
            out.append((int(p[1], 16), int(p[2], 16)))
    return out

def union(rs):
    rs = sorted(rs); out = []
    for a, b in rs:
        if out and a <= out[-1][1]: out[-1][1] = max(out[-1][1], b)
        else: out.append([a, b])
    return out

def subtract(rs, cut):
    out = []; j = 0
    for a, b in rs:
        while j < len(cut) and cut[j][1] <= a: j += 1
        k = j
        while a < b:
            if k < len(cut) and cut[k][0] < b:
                if cut[k][0] > a: out.append([a, cut[k][0]])
                a = max(a, cut[k][1]); k += 1
            else:
                out.append([a, b]); break
    return out

masks = union([r for f in sys.argv[1:] for r in ranges(f, 'mask')])
keeps = union([r for f in sys.argv[1:] for r in ranges(f, 'keep')])
res = subtract(masks, keeps)
words = sum(b - a for a, b in res) // 4
print('# gs_mask.py %s: %d ranges, %d words (%.1f KB); kept (read by game code): %d words' % (' '.join(sys.argv[1:]), len(res), words, words / 256,
      sum(b - a for a, b in keeps) // 4))
for a, b in res:
    print('mask %06x %06x' % (a, b))
