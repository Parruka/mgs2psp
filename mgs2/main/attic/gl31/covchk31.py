"""covchk31.py REFDIR NEWDIR GERDIR FRAME:BEFORE [FRAME:BEFORE ...] (gl31 G3, the lead's update): the shots outside vb 29,490-31,400
whose frame's list arm X changed, new run against gl30's emulator run (shot_<FRAME>.bmp in both): the pixels whose luma changed by 2+,
6+ and more than 8 levels, inside and outside the screen coverage of the frame's 0x42 targets (GERDIR/f<FRAME>.ger from the twin with
the delivered keys: every batch with the depth test, blend 0x42 and kind XFORM; its own triangles), widened by 2 px, of this frame and of
the frame before (the motion trail samples the other buffer).  The coverage code is build_gl30/picchk.py's (ger_masks, dil).  Counts
only: no picture leaves the container."""
import sys, os, struct
import numpy as np
W, H = 480, 272
def load_bmp(p):
    b = open(p, 'rb').read()
    off = struct.unpack_from('<I', b, 10)[0]; w, h = struct.unpack_from('<ii', b, 18); bpp = struct.unpack_from('<H', b, 28)[0]
    assert bpp == 24 and w == W and abs(h) == H, p
    rs = (w * 3 + 3) & ~3
    a = np.frombuffer(b, np.uint8, rs * abs(h), off).reshape(abs(h), rs)[:, :w * 3].reshape(abs(h), w, 3)
    if h > 0: a = a[::-1]
    a = a.astype(np.int32)
    return (a[:, :, 2] * 299 + a[:, :, 1] * 587 + a[:, :, 0] * 114) // 1000
def targets(b):
    magic, vb, nb, nv, nt, nd, ob, ov, ot, od = struct.unpack_from('<10I', b, 0); ids = []
    for i in range(nb):
        r = struct.unpack_from('<HBB4B6B2B4h4I', b, ob + 40 * i)
        if (r[2] & 16) and (r[2] & 4) and ((r[7] | r[8] << 2 | r[9] << 4 | r[10] << 6) & 0xff) == 0x42 and r[14] == 1: ids.append(i)
    return vb, nb, ids
def ger_masks(b, ids):
    magic, vb, nb, nv, nt, nd, ob, ov, ot, od = struct.unpack_from('<10I', b, 0)
    m = np.zeros((H, W), bool)
    yy, xx = np.mgrid[0:H, 0:W]; px = xx + 0.5; py = yy + 0.5
    for i in ids:
        r = struct.unpack_from('<HBB4B6B2B4h4I', b, ob + 40 * i); first = r[20]
        xo = od + first; x = struct.unpack_from('<36f', b, xo); nvx, nst = struct.unpack_from('<2I', b, xo + 164)
        V = [struct.unpack_from('<HHI4h', b, xo + 188 + 16 * k) for k in range(nvx)]
        runs = [struct.unpack_from('<2H', b, xo + 188 + 16 * nvx + 4 * k) for k in range(nst)]
        Wm, Pm, vs, vo = x[0:12], x[12:28], x[28:31], x[31:34]
        S = []
        for q in V:
            X, Y, Z = q[3] / 32768.0, q[4] / 32768.0, q[5] / 32768.0
            e = [X * Wm[j] + Y * Wm[3 + j] + Z * Wm[6 + j] + Wm[9 + j] for j in range(3)]
            cx = e[0] * Pm[0] + e[1] * Pm[4] + e[2] * Pm[8] + Pm[12]; cy = e[0] * Pm[1] + e[1] * Pm[5] + e[2] * Pm[9] + Pm[13]
            cw = e[0] * Pm[3] + e[1] * Pm[7] + e[2] * Pm[11] + Pm[15]
            S.append(None if cw <= 1e-6 else (vs[0] * cx / cw + vo[0] - 1808, vs[1] * cy / cw + vo[1] - 1912))
        for (f0, cnt) in runs:
            for k in range(f0 + 2, f0 + cnt):
                t = (S[k - 2], S[k - 1], S[k])
                if None in t: continue
                (x0, y0), (x1, y1), (x2, y2) = t
                d = (x1 - x0) * (y2 - y0) - (x2 - x0) * (y1 - y0)
                if abs(d) < 1e-9: continue
                lx, hx = max(int(min(x0, x1, x2)) - 1, 0), min(int(max(x0, x1, x2)) + 2, W)
                ly, hy = max(int(min(y0, y1, y2)) - 1, 0), min(int(max(y0, y1, y2)) + 2, H)
                if lx >= hx or ly >= hy: continue
                X_, Y_ = px[ly:hy, lx:hx], py[ly:hy, lx:hx]
                a = ((x1 - X_) * (y2 - Y_) - (x2 - X_) * (y1 - Y_)) / d; c = ((x2 - X_) * (y0 - Y_) - (x0 - X_) * (y2 - Y_)) / d
                e3 = 1 - a - c
                m[ly:hy, lx:hx] |= (a >= 0) & (c >= 0) & (e3 >= 0)
    return m
def dil(m, k):
    o = m.copy()
    for dy in range(-k, k + 1):
        for dx in range(-k, k + 1):
            s = np.roll(np.roll(m, dy, 0), dx, 1)
            if dy > 0: s[:dy] = False
            if dy < 0: s[dy:] = False
            if dx > 0: s[:, :dx] = False
            if dx < 0: s[:, dx:] = False
            o |= s
    return o
ref, new, gd = sys.argv[1:4]
for a in sys.argv[4:]:
    f, p = (int(x) for x in a.split(':')); n = 'shot_%d.bmp' % f
    d = np.abs(load_bmp(os.path.join(new, n)) - load_bmp(os.path.join(ref, n)))
    mm = np.zeros((H, W), bool); info = []
    for v in (f, p):
        b = open('%s/f%d.ger' % (gd, v), 'rb').read(); vb, nb, ids = targets(b); m = dil(ger_masks(b, ids), 2); mm |= m
        info.append('frame %d: %d batches, %d targets, coverage %d px' % (vb, nb, len(ids), int(m.sum())))
    print('%s: luma differs by 1+ in %d px, 2+ %d, 6+ %d, over 8 %d (largest %d); outside the 0x42 coverage of this frame and the one before: '
          '1+ %d, 2+ %d, 6+ %d, over 8 %d; %s' % (n, int((d >= 1).sum()), int((d >= 2).sum()), int((d >= 6).sum()), int((d > 8).sum()), int(d.max()),
          int(((d >= 1) & ~mm).sum()), int(((d >= 2) & ~mm).sum()), int(((d >= 6) & ~mm).sum()), int(((d > 8) & ~mm).sum()), '; '.join(info)))
