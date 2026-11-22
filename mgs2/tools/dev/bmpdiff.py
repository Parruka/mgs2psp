#!/usr/bin/env python3
# bmpdiff.py a.bmp b.bmp: 24-bit BMPs of the same size: pixels that differ, the largest level difference, the rows and columns they span
import sys, struct
a = open(sys.argv[1], 'rb').read(); b = open(sys.argv[2], 'rb').read()
off = struct.unpack_from('<I', a, 10)[0]; w, h = struct.unpack_from('<ii', a, 18); st = (w * 3 + 3) & ~3
px = 0; mx = 0; rows = set(); cols = set(); hist = {}
for y in range(abs(h)):
    for x in range(w):
        o = off + y * st + 3 * x
        d = max(abs(a[o + k] - b[o + k]) for k in range(3))
        if d:
            px += 1; mx = max(mx, d); hist[d] = hist.get(d, 0) + 1
            rows.add(abs(h) - 1 - y if h > 0 else y); cols.add(x)
print('%d x %d: %d pixels differ, largest %d levels, by size %s; rows %s..%s, columns %s..%s' % (w, abs(h), px, mx, dict(sorted(hist.items())),
      min(rows) if rows else '-', max(rows) if rows else '-', min(cols) if cols else '-', max(cols) if cols else '-'))
