#!/usr/bin/env python3
# pairshots.py <A dir> <B dir> <out dir> <name A> <name B> <vb>...: two runs' shots side by side for Ffm (game pictures: personal use,
# Ffm's PC folder only).  Per shot: vb_<n>_1_<name A>.png and vb_<n>_2_<name B>.png (480 x 272 as the PSP shows them),
# vb_<n>_3_where.png (black where equal; where a pixel differs, 32 + 32 x its largest channel difference, so 7 levels or more is white),
# vb_<n>_4_zoom.png (A left, B right, x4, the 96 x 68 box holding the most difference in total).  Prints each shot's figures.
import sys, os
import numpy as np
from PIL import Image
A, B, O, NA, NB = sys.argv[1:6]; os.makedirs(O, exist_ok=True)
CW, CH = 96, 68
for vb in sys.argv[6:]:
    a = Image.open(os.path.join(A, 'shot_%s.bmp' % vb)).convert('RGB'); b = Image.open(os.path.join(B, 'shot_%s.bmp' % vb)).convert('RGB')
    a.save(os.path.join(O, 'vb_%s_1_%s.png' % (vb, NA))); b.save(os.path.join(O, 'vb_%s_2_%s.png' % (vb, NB)))
    pa = np.asarray(a, dtype=np.int16); pb = np.asarray(b, dtype=np.int16)
    d = np.abs(pa - pb).max(axis=2)                     # largest channel difference per pixel
    h, w = d.shape
    m = np.where(d > 0, np.minimum(255, 32 + 32 * d.astype(np.int32)), 0).astype(np.uint8)
    Image.fromarray(m, 'L').save(os.path.join(O, 'vb_%s_3_where.png' % vb))
    s = np.zeros((h + 1, w + 1), dtype=np.int64); s[1:, 1:] = d.astype(np.int64).cumsum(0).cumsum(1)
    box = s[CH:, CW:] - s[:-CH, CW:] - s[CH:, :-CW] + s[:-CH, :-CW]   # box sums, top-left corners (0..h-CH, 0..w-CW)
    y0, x0 = np.unravel_index(int(box.argmax()), box.shape)
    za = a.crop((x0, y0, x0 + CW, y0 + CH)).resize((CW * 4, CH * 4), Image.NEAREST)
    zb = b.crop((x0, y0, x0 + CW, y0 + CH)).resize((CW * 4, CH * 4), Image.NEAREST)
    z = Image.new('RGB', (CW * 8 + 8, CH * 4), (255, 255, 255)); z.paste(za, (0, 0)); z.paste(zb, (CW * 4 + 8, 0))
    z.save(os.path.join(O, 'vb_%s_4_zoom.png' % vb))
    n = d.size
    print('%s mean %.2f  >0 %.2f%%  >8 %.2f%%  >16 %.2f%%  >32 %.2f%%  max %d  zoom box %d,%d' % (
        vb, d.mean(), 100.0 * (d > 0).sum() / n, 100.0 * (d > 8).sum() / n, 100.0 * (d > 16).sum() / n, 100.0 * (d > 32).sum() / n,
        int(d.max()), x0, y0))
