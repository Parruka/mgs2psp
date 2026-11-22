import sys, struct, numpy as np
# cov.py GER PSP.bmp EMU.bmp T b...: per batch (df0), its screen coverage (float model, gerana's rule) and the pixels there where the PSP
# is lighter than the emulator by more than T luma (counts only); also the pixels it covers first-hand among all listed.
exec(open(sys.argv[0].replace('p29/cov.py', 'gerana.py')).read().split('fn, psp, emu = sys.argv[1]')[0])
fn, psp, emu, T = sys.argv[1], sys.argv[2], sys.argv[3], float(sys.argv[4]); ids = [int(a) for a in sys.argv[5:]]
b = open(fn, 'rb').read()
magic, vb, nb, nv, nt, nd, ob, ov, ot, od, w, h = struct.unpack_from('<12I', b, 0); zsc, zlo, zhi = struct.unpack_from('<3f', b, 48)
zs, zo = depth_map(zlo, zhi)
A = load_bmp(psp); Bm = load_bmp(emu); lit = A > Bm + T
print('frame %d: PSP lighter by > %g: %d px' % (vb, T, int(lit.sum())))
for i in ids:
    r = struct.unpack_from('<HBB4B6B2B4h4I', b, ob + 40 * i); tex, flags, kind, first = r[0], r[2], r[14], r[20]
    if kind != 1: print('  b%d: not df0' % i); continue
    xo = od + first; x = struct.unpack_from('<36f', b, xo); nvx, nst = struct.unpack_from('<2I', b, xo + 164)
    V = [struct.unpack_from('<HHI4h', b, xo + 188 + 16 * k) for k in range(nvx)]
    runs = [struct.unpack_from('<2H', b, xo + 188 + 16 * nvx + 4 * k) for k in range(nst)]
    Wm = [f24(c) for c in x[0:12]]; Pm = [f24(c) for c in x[12:28]]; vs, vo = x[28:31], x[31:34]
    S = []
    for q in V:
        X, Y, Z = q[3] / 32768.0, q[4] / 32768.0, q[5] / 32768.0
        e = [X * Wm[j] + Y * Wm[3 + j] + Z * Wm[6 + j] + Wm[9 + j] for j in range(3)]
        cx = e[0] * Pm[0] + e[1] * Pm[4] + e[2] * Pm[8] + Pm[12]; cy = e[0] * Pm[1] + e[1] * Pm[5] + e[2] * Pm[9] + Pm[13]
        cw = e[0] * Pm[3] + e[1] * Pm[7] + e[2] * Pm[11] + Pm[15]
        S.append((f24(vs[0]) * cx / cw + f24(vo[0]) - 1808, f24(vs[1]) * cy / cw + f24(vo[1]) - 1912))
    m = np.zeros((H, W), bool)
    for (f0, cnt) in runs:
        for k in range(f0 + 2, f0 + cnt): cover(m, (S[k - 2], S[k - 1], S[k]))
    print('  b%d (tex %d, flags %02x): covers %d px, PSP lighter there %d' % (i, tex, flags, int(m.sum()), int((m & lit).sum())))
