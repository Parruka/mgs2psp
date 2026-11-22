#!/usr/bin/env python3
"""shotdiff.py <dirA> <dirB> [names...]: the harness's 480 x 272 BMP shots of two runs, pixel for pixel.
Per shot: md5 equal or not, pixels that differ, the largest channel difference, a histogram of each differing pixel's largest
channel difference, the rows and columns that hold differences, and the signed mean per channel (B - A). Game pictures: container only."""
import sys, os, hashlib
import numpy as np
from PIL import Image

W, H = 480, 272
BINS = [(1, 1), (2, 2), (3, 4), (5, 8), (9, 16), (17, 32), (33, 255)]


def load(p):
    return np.asarray(Image.open(p).convert('RGB'))[:H, :W].astype(np.int16)


def runs(idx):
    """compress a sorted index list into 'a-b' ranges"""
    out = []
    for i in idx:
        if out and i == out[-1][1] + 1:
            out[-1][1] = i
        else:
            out.append([i, i])
    return ','.join('%d' % a if a == b else '%d-%d' % (a, b) for a, b in out)


a_dir, b_dir = sys.argv[1], sys.argv[2]
names = sys.argv[3:] or sorted(n for n in os.listdir(a_dir) if n.endswith('.bmp'))
for n in names:
    pa, pb = os.path.join(a_dir, n), os.path.join(b_dir, n)
    if not os.path.exists(pb):
        print('%s: missing in %s' % (n, b_dir)); continue
    ma = hashlib.md5(open(pa, 'rb').read()).hexdigest()[:8]
    mb = hashlib.md5(open(pb, 'rb').read()).hexdigest()[:8]
    a, b = load(pa), load(pb)
    d = b - a
    m = np.abs(d).max(axis=2)
    nd = int((m > 0).sum())
    print('%s: md5 %s / %s%s; %d of %d pixels differ (%.3f%%), largest %d levels' % (
        n, ma, mb, ' EQUAL' if ma == mb else '', nd, W * H, 100.0 * nd / (W * H), int(m.max())))
    if nd:
        print('   by largest channel difference: ' + ', '.join('%s %d' % ('%d' % lo if lo == hi else '%d-%d' % (lo, hi), int(((m >= lo) & (m <= hi)).sum())) for lo, hi in BINS))
        sel = m > 0
        print('   mean B-A on differing pixels (R, G, B): %.2f %.2f %.2f' % tuple(float(d[..., c][sel].mean()) for c in range(3)))
        rows = np.nonzero(sel.any(axis=1))[0]
        cols = np.nonzero(sel.any(axis=0))[0]
        print('   rows %d: %s' % (len(rows), runs(rows.tolist())[:300]))
        print('   cols %d: %s' % (len(cols), runs(cols.tolist())[:300]))
