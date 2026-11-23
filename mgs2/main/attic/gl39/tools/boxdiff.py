#!/usr/bin/env python3
"""boxdiff.py DIR x0 x1 y0 y1 (python3 -I): successive shot_*.bmp of one run, pixels that differ inside a box (image data)."""
import sys, os, struct
def bmp(p):
    d = open(p, 'rb').read(); off, = struct.unpack_from('<I', d, 10); w, h, pl, bpp = struct.unpack_from('<iiHH', d, 18)
    top = h < 0; h = abs(h); bs = bpp // 8; row = (w * bs + 3) & ~3
    return w, h, bs, [d[off + (y if top else h - 1 - y) * row: off + (y if top else h - 1 - y) * row + w * bs] for y in range(h)]
D = sys.argv[1]; x0, x1, y0, y1 = map(int, sys.argv[2:6]); prev = None; pn = ''
for f in sorted((f for f in os.listdir(D) if f.startswith('shot_')), key=lambda s: int(s[5:-4])):
    v = int(f[5:-4])
    if v < 26000 or v > 28700: continue
    w, h, bs, R = bmp(D + '/' + f)
    if prev:
        n = sum(1 for y in range(y0, y1 + 1) for x in range(x0, x1 + 1) if R[y][x * bs:x * bs + 3] != prev[y][x * bs:x * bs + 3])
        print('%s vs %s: %d of %d pixels differ in the box' % (pn, f, n, (x1 - x0 + 1) * (y1 - y0 + 1)))
    prev = R; pn = f
