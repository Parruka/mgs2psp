"""covchk2.py ODIR XDIR GERDIR FROM TO [N ...] (gl31 G3 extra, the lead's update): the same frames drawn with arm O (ODIR: the delivered
EBOOT with MGS2_GL_DZ=29490:31400:X, gl30's setting) and with arm X on the whole run (XDIR: the delivered cfg), a picture every 6 vblanks
(MGS2_GL_SHOT_RANGE, the same schedule in both runs): shot_<frame>.bmp for FROM <= frame <= TO.  Per frame: the pixels that differ (any
channel; luma by 1+, 2+, 6+, over 8) and how many of them lie outside the screen coverage of the 0x42 targets (GERDIR/f<frame>.ger from
the twin with the delivered keys; each target's own triangles, widened by 2 px) of this frame and the N frames before it (N = 1 is
build_gl30/picchk.py's rule: the motion trail samples the other buffer; larger N: the trail of a trail).  Counts only."""
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
od, xd, gd = sys.argv[1:4]; lo, hi = int(sys.argv[4]), int(sys.argv[5]); NS = [int(x) for x in sys.argv[6:]] or [1]
fr = sorted(int(n[5:-4]) for n in os.listdir(xd) if n.startswith('shot_') and lo <= int(n[5:-4]) <= hi)
cov = {}
def mask(v):
    if v not in cov:
        p = '%s/f%d.ger' % (gd, v)
        if not os.path.exists(p): cov[v] = None
        else:
            b = open(p, 'rb').read(); vb, nb, ids = targets(b); cov[v] = (dil(ger_masks(b, ids), 2), len(ids))
    return cov[v]
T = {'frames': 0, 'differ': 0, 'px': 0, 'l1': 0, 'l2': 0, 'l6': 0, 'l8': 0, 'max': 0, 'tg': 0}; OUT = {n: [0, 0, 0, 0, 0] for n in NS}; noger = []
for f in fr:
    n = 'shot_%d.bmp' % f; a = open(os.path.join(od, n), 'rb').read(); b = open(os.path.join(xd, n), 'rb').read()
    T['frames'] += 1
    m0 = mask(f)
    if m0 is None: noger.append(f)
    else: T['tg'] += m0[1]
    if a == b: continue
    d = np.abs(load_bmp(os.path.join(xd, n)) - load_bmp(os.path.join(od, n)))
    T['differ'] += 1; T['l1'] += int((d >= 1).sum()); T['l2'] += int((d >= 2).sum()); T['l6'] += int((d >= 6).sum()); T['l8'] += int((d > 8).sum()); T['max'] = max(T['max'], int(d.max()))
    row = '%s: luma differs 1+ %d, 2+ %d, 6+ %d, over 8 %d (largest %d); targets %s' % (n, int((d >= 1).sum()), int((d >= 2).sum()), int((d >= 6).sum()), int((d > 8).sum()), int(d.max()), m0[1] if m0 else 'no dump')
    for N in NS:
        mm = np.zeros((H, W), bool)
        for k in range(0, N + 1):
            # the frames before: the dumps present below f, nearest first
            pass
        prev = sorted((v for v in (int(x[1:-4]) for x in os.listdir(gd) if x.endswith('.ger')) if v <= f), reverse=True)[:N + 1]
        for v in prev:
            mk = mask(v)
            if mk: mm |= mk[0]
        o = [int(((d >= 1) & ~mm).sum()), int(((d >= 2) & ~mm).sum()), int(((d >= 6) & ~mm).sum()), int(((d > 8) & ~mm).sum())]
        for i in range(4): OUT[N][i] += o[i]
        if o[0]: OUT[N][4] += 1
        row += '; outside the coverage of this frame and %d before: 1+ %d, 2+ %d, 6+ %d, over 8 %d' % (N, o[0], o[1], o[2], o[3])
    print(row)
print('frames %d (%d..%d), 0x42 targets in them %d, pictures that differ %d; luma differs 1+ %d px, 2+ %d, 6+ %d, over 8 %d (largest %d); frames with no dump %s' % (
    T['frames'], fr[0], fr[-1], T['tg'], T['differ'], T['l1'], T['l2'], T['l6'], T['l8'], T['max'], noger))
for N in NS: print('outside the 0x42 coverage of the frame and the %d before it: 1+ %d px, 2+ %d, 6+ %d, over 8 %d, in %d pictures' % (N, *OUT[N]))
