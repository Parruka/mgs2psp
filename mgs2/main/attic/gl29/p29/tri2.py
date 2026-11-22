import sys, struct, numpy as np
# tri2.py GER PSP EMU T: per paired 0x42 decal (dq.py's pairing): its triangles' largest GE z span and screen size, the lead (GE, at
# vertices), its coverage and the pixels there where the PSP is lighter by > T (float model; counts only)
G = sys.argv[0].replace('p29/tri2.py', 'gerana.py'); args = sys.argv[1:]
exec(open(G).read().split('fn, psp, emu = sys.argv[1]')[0])
fn, psp, emu, T = args[0], args[1], args[2], float(args[3])
b = open(fn, 'rb').read()
magic, vb, nb, nv, nt, nd, ob, ov, ot, od, w, h = struct.unpack_from('<12I', b, 0); zsc, zlo, zhi = struct.unpack_from('<3f', b, 48)
zs, zo = depth_map(zlo, zhi)
lit = load_bmp(psp) > load_bmp(emu) + T
Bt = []
for i in range(nb):
    r = struct.unpack_from('<HBB4B6B2B4h4I', b, ob + 40 * i); tex, flags, ba, bb, bc, bd, kind, first = r[0], r[2], r[7], r[8], r[9], r[10], r[14], r[20]
    if kind != 1: Bt.append(None); continue
    xo = od + first; x = struct.unpack_from('<36f', b, xo); nvx, nst = struct.unpack_from('<2I', b, xo + 164)
    V = [struct.unpack_from('<HHI4h', b, xo + 188 + 16 * k)[3:6] for k in range(nvx)]
    runs = [struct.unpack_from('<2H', b, xo + 188 + 16 * nvx + 4 * k) for k in range(nst)]
    Bt.append(dict(i=i, tex=tex, flags=flags, sig=ba | bb << 2 | bc << 4 | bd << 6, x=x, V=V, runs=runs))
def scr(d):
    x = d['x']; Wm = [f24(c) for c in x[0:12]]; Pm = [f24(c) for c in x[12:28]]; vs, vo = x[28:31], x[31:34]
    vz = f24(vs[2] * zs); oz = f24(vo[2] * zs + zo); S = []
    for q in d['V']:
        X, Y, Z = q[0] / 32768.0, q[1] / 32768.0, q[2] / 32768.0
        e = [X * Wm[j] + Y * Wm[3 + j] + Z * Wm[6 + j] + Wm[9 + j] for j in range(3)]
        cx = e[0] * Pm[0] + e[1] * Pm[4] + e[2] * Pm[8] + Pm[12]; cy = e[0] * Pm[1] + e[1] * Pm[5] + e[2] * Pm[9] + Pm[13]
        cz = e[0] * Pm[2] + e[1] * Pm[6] + e[2] * Pm[10] + Pm[14]; cw = e[0] * Pm[3] + e[1] * Pm[7] + e[2] * Pm[11] + Pm[15]
        S.append((f24(vs[0]) * cx / cw + f24(vo[0]) - 1808, f24(vs[1]) * cy / cw + f24(vo[1]) - 1912, vz * cz / cw + oz))
    return S
print('frame %d (map %.0f..%.0f, zs %.4g): PSP lighter > %g: %d px' % (vb, zlo, zhi, zs, T, int(lit.sum())))
for d in Bt:
    if not d or d['sig'] != 0x42 or not (d['flags'] & 16): continue
    pair = None
    for j in range(d['i'] - 1, max(-1, d['i'] - 80), -1):
        c = Bt[j]
        if not c or c['sig'] == 0x42 or not (c['flags'] & 32) or len(c['V']) != len(d['V']) or c['x'][0:12] != d['x'][0:12]: continue
        sb, sd = sorted(c['V']), sorted(d['V']); off = tuple(sd[0][k] - sb[0][k] for k in range(3))
        if all(tuple(p[k] - q[k] for k in range(3)) == off for p, q in zip(sd, sb)) and max(map(abs, off)) <= 64: pair = (c, off); break
    if not pair: continue
    c, off = pair; S = scr(d); Sb = scr(c); idx = {v: k for k, v in enumerate(c['V'])}
    lead = [S[k][2] - Sb[idx[tuple(p[m] - off[m] for m in range(3))]][2] for k, p in enumerate(d['V'])]
    m = np.zeros((H, W), bool); span = 0; area = 0; ntri = 0
    for (f0, cnt) in d['runs']:
        for k in range(f0 + 2, f0 + cnt):
            t = (S[k - 2], S[k - 1], S[k]); ntri += 1
            span = max(span, max(p[2] for p in t) - min(p[2] for p in t))
            area = max(area, abs((t[1][0] - t[0][0]) * (t[2][1] - t[0][1]) - (t[2][0] - t[0][0]) * (t[1][1] - t[0][1])) / 2)
            cover(m, ((t[0][0], t[0][1]), (t[1][0], t[1][1]), (t[2][0], t[2][1])))
    cv = int(m.sum()); lt = int((m & lit).sum())
    print('  b%d tex %d over b%d: %d tris, largest z span %.0f GE, largest tri %.0f px2; lead %.2f..%.2f GE (lead / span %.2g); covers %d, lighter %d (%.0f%%)' % (
        d['i'], d['tex'], c['i'], ntri, span, area, min(lead), max(lead), min(lead) / span if span else 0, cv, lt, 100.0 * lt / cv if cv else 0))
