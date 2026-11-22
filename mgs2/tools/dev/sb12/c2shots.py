#!/usr/bin/env python3
# c2shots.py <off dir> <on dir> <out dir> <vb>...: for Ffm's look at C2 (the people) (game pictures: personal use, Ffm's PC folder only): per shot
# vb_<n>_1_today.png and vb_<n>_2_c2.png (480 x 272 as the PSP shows them), vb_<n>_3_where.png (white where a pixel differs, brighter
# for a larger difference: 1 level shows as 64 of 255), and vb_<n>_4_zoom.png (both forms side by side, x4, around the largest difference)
import sys, os
from PIL import Image, ImageChops
A, B, O = sys.argv[1], sys.argv[2], sys.argv[3]; os.makedirs(O, exist_ok=True)
for vb in sys.argv[4:]:
    a = Image.open(os.path.join(A, 'shot_%s.bmp' % vb)).convert('RGB'); b = Image.open(os.path.join(B, 'shot_%s.bmp' % vb)).convert('RGB')
    a.save(os.path.join(O, 'vb_%s_1_today.png' % vb)); b.save(os.path.join(O, 'vb_%s_2_c2.png' % vb))
    d = ImageChops.difference(a, b); g = Image.eval(d.convert('L', (1/3, 1/3, 1/3, 0)) if False else d.split()[0], lambda v: v)
    w, h = a.size; pa, pb = a.load(), b.load()
    m = Image.new('L', (w, h)); pm = m.load(); best = (0, w // 2, h // 2)
    for y in range(h):
        for x in range(w):
            dd = max(abs(pa[x, y][k] - pb[x, y][k]) for k in range(3))
            if dd: pm[x, y] = min(255, 32 + 32 * dd)
            if dd > best[0]: best = (dd, x, y)
    m.save(os.path.join(O, 'vb_%s_3_where.png' % vb))
    cw, ch = 96, 68; x0 = max(0, min(w - cw, best[1] - cw // 2)); y0 = max(0, min(h - ch, best[2] - ch // 2))
    za = a.crop((x0, y0, x0 + cw, y0 + ch)).resize((cw * 4, ch * 4), Image.NEAREST); zb = b.crop((x0, y0, x0 + cw, y0 + ch)).resize((cw * 4, ch * 4), Image.NEAREST)
    z = Image.new('RGB', (cw * 8 + 8, ch * 4), (255, 255, 255)); z.paste(za, (0, 0)); z.paste(zb, (cw * 4 + 8, 0)); z.save(os.path.join(O, 'vb_%s_4_zoom.png' % vb))
    print(vb, 'largest', best[0], 'at', best[1], best[2], 'zoom box', x0, y0, cw, ch)
