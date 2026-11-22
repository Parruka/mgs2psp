#!/usr/bin/env python3
"""rowdiff.py <a.bmp> <b.bmp>: per row with differences, the count, the columns (ranges) and the largest difference. Container only."""
import sys
import numpy as np
from PIL import Image
W, H = 480, 272
a = np.asarray(Image.open(sys.argv[1]).convert('RGB'))[:H, :W].astype(np.int16)
b = np.asarray(Image.open(sys.argv[2]).convert('RGB'))[:H, :W].astype(np.int16)
m = np.abs(b - a).max(axis=2)
def runs(idx):
    out = []
    for i in idx:
        if out and i == out[-1][1] + 1: out[-1][1] = i
        else: out.append([i, i])
    return ','.join('%d' % x if x == y else '%d-%d' % (x, y) for x, y in out)
for y in range(H):
    c = np.nonzero(m[y])[0]
    if len(c):
        ex = c[0]
        print('row %3d: %3d px, max %3d, cols %s | A %s B %s' % (y, len(c), m[y].max(), runs(c.tolist())[:120], tuple(a[y, ex]), tuple(b[y, ex])))
