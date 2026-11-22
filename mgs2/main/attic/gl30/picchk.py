import sys, os, re, struct, collections
import numpy as np
# picchk.py REFDIR NEWDIR XSLIST GERDIR... (gl30 G3, 2026-11-20): the emulator's pictures of gl30 (NEW) against gl29x's (REF), same
# names (shot_<frame vb>.bmp, live_<vb>.bmp, every picture in both).  Per picture: pixels whose luma (299 R + 587 G + 114 B) / 1000
# changed by 2+, 6+ and more than 8 levels, and where they lie: inside the screen coverage of the frame's subset (XS) decals (XSLIST:
# 'vb batch offset ok' from gerl/cmp.py), or of the frame's and the frame before's (the motion trail samples the other buffer), each
# dilated by 2 px; else outside.  The coverage: each XS batch's triangles from its GER file (the GE's transform in float: world, proj,
# viewport as gelist sends them, the screen at 1808, 1912), filled by their pixel centres.  Counts only: no picture leaves the container.
W, H = 480, 272
def load_bmp(p):
    b = open(p, 'rb').read()
    off = struct.unpack_from('<I', b, 10)[0]; w, h = struct.unpack_from('<ii', b, 18); bpp = struct.unpack_from('<H', b, 28)[0]
    assert bpp == 24 and w == W and abs(h) == H, p
    rs = (w * 3 + 3) & ~3
    a = np.frombuffer(b, np.uint8, rs * abs(h), off).reshape(abs(h), rs)[:, :w * 3].reshape(abs(h), w, 3)
    if h > 0: a = a[::-1]
    a = a.astype(np.int32)
    return (a[:, :, 2] * 299 + a[:, :, 1] * 587 + a[:, :, 0] * 114) // 1000   # BMP rows are B, G, R
def ger_masks(fn, ids):
    b = open(fn, 'rb').read()
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
ref, new, xsl = sys.argv[1], sys.argv[2], sys.argv[3]; gd = sys.argv[4:]
XS = collections.defaultdict(list)
for f in xsl.split(','):
    for l in open(f):
        v, i, o, ok = l.split(); XS[int(v)].append(int(i))
def gerf(vb):
    for d in gd:
        p = '%s/f%d.ger' % (d, vb)
        if os.path.exists(p): return p
    return None
MC = {}
def mask(vb):
    if vb not in MC:
        MC[vb] = dil(ger_masks(gerf(vb), XS[vb]), 2) if XS.get(vb) and gerf(vb) else (None if XS.get(vb) else np.zeros((H, W), bool))
    return MC[vb]
names = sorted(set(os.listdir(ref)) & set(os.listdir(new)), key=lambda n: (n.split('_')[0], int(re.sub(r'\D', '', n) or 0)))
only = sorted(set(os.listdir(ref)) ^ set(os.listdir(new)))
T = collections.Counter(); rows = []; worst = []
for n in names:
    if not n.endswith('.bmp'): continue
    a, b = load_bmp(os.path.join(ref, n)), load_bmp(os.path.join(new, n)); d = np.abs(b - a)
    k = n.split('_')[0]; vb = int(re.sub(r'\D', '', n))
    T[k + ' pictures'] += 1
    if not d.any(): T[k + ' identical'] += 1; continue
    T[k + ' differing'] += 1
    c2, c6, c8 = int((d >= 2).sum()), int((d >= 6).sum()), int((d > 8).sum())
    if k == 'shot':
        m0, m1 = mask(vb), mask(vb - 2)
        if m0 is None or m1 is None: rows.append((vb, c2, c6, c8, 'no GER')); T['shot without coverage'] += 1; continue
        mm = m0 | m1
        o2, o6, o8 = int(((d >= 2) & ~mm).sum()), int(((d >= 6) & ~mm).sum()), int(((d > 8) & ~mm).sum())
        i2c = int(((d >= 2) & m0).sum())
        rows.append((vb, c2, c6, c8, i2c, o2, o6, o8, len(XS.get(vb, [])), int(d.max())))
        T['shot px >=2'] += c2; T['shot px >=6'] += c6; T['shot px >8'] += c8; T['shot px >=2 outside'] += o2; T['shot px >=6 outside'] += o6
        T['shot px >8 outside'] += o8; T['shot px >=2 in this frame XS'] += i2c
        worst.append((c8, vb))
    else:
        rows.append((n, c2, c6, c8, int(d.max()))); T[k + ' px >8'] += c8
print('pictures only in one folder:', only[:10], len(only))
for k in sorted(T): print('%-34s %d' % (k, T[k]))
print('shots with the most px over 8 levels (n, frame vb):', sorted(worst, reverse=True)[:8])
xsf = [v for v in XS if XS[v]]
print('frames with XS decals: %d; shot frames with XS: %d' % (len(xsf), sum(1 for r in rows if isinstance(r[0], int) and r[0] in XS)))
print('per differing picture: shot (frame vb, px>=2, >=6, >8, px>=2 inside this frame XS, px>=2 / >=6 / >8 outside XS of this and the frame before, XS decals, max |d|); live (name, >=2, >=6, >8, max)')
for r in rows: print('  ', r)
