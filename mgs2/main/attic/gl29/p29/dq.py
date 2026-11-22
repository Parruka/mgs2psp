import sys, struct
# dq.py GER...: for each 0x42 df0 (XFORM) batch, the earlier Z-writing df0 batch whose vertices are its own minus one constant s16
# offset (same world matrix, same vertex count): the decal's lead per matched vertex, in z/w (dq = q_base - q_decal, relative to q)
# and in GE units after the frame's depth map; plus vs2, zs and S = vs2*zs.  Prints numbers only.
def f24(x):
    u = struct.unpack('<I', struct.pack('<f', x))[0] & 0xffffff00
    return struct.unpack('<f', struct.pack('<I', u))[0]
def depth_map(lo, hi):
    if lo > hi: lo, hi = 0.0, 16777215.0
    m = (hi - lo) / 16; a = lo - m; b = hi + m
    if a < 0: a = 0
    if b <= a: b = a + 1
    k = 65535.0 / (b - a); return k, -a * k
for fn in sys.argv[1:]:
    b = open(fn, 'rb').read()
    magic, vb, nb, nv, nt, nd, ob, ov, ot, od, w, h = struct.unpack_from('<12I', b, 0); zsc, zlo, zhi = struct.unpack_from('<3f', b, 48)
    zs, zo = depth_map(zlo, zhi)
    B = []
    for i in range(nb):
        r = struct.unpack_from('<HBB4B6B2B4h4I', b, ob + 40 * i)
        tex, prim, flags, ztst, atst, aref, afail, ba, bb, bc, bd, fix, tfx, twop, kind = r[:15]; first = r[20]
        sig = ba | bb << 2 | bc << 4 | bd << 6
        if kind != 1: B.append(None); continue
        xo = od + first; x = struct.unpack_from('<36f', b, xo); nvx, nst = struct.unpack_from('<2I', b, xo + 164)
        V = [struct.unpack_from('<HHI4h', b, xo + 188 + 16 * k)[3:6] for k in range(nvx)]
        B.append(dict(i=i, tex=tex, flags=flags, sig=sig, world=x[0:12], proj=x[12:28], vs=x[28:31], vo=x[31:34], V=V))
    print('%s vb %d map %.0f..%.0f zs %.6g zo %.6g' % (fn.split('/')[-1], vb, zlo, zhi, zs, zo))
    for d in B:
        if not d or d['sig'] != 0x42 or not (d['flags'] & 16): continue
        best = None
        for j in range(d['i'] - 1, max(-1, d['i'] - 80), -1):
            c = B[j]
            if not c or c['sig'] == 0x42 or not (c['flags'] & 32) or len(c['V']) != len(d['V']) or c['world'] != d['world']: continue
            sb = sorted(c['V']); sd = sorted(d['V'])
            off = tuple(sd[0][k] - sb[0][k] for k in range(3))
            if all(tuple(p[k] - q[k] for k in range(3)) == off for p, q in zip(sd, sb)) and max(map(abs, off)) <= 64:
                best = (c, off); break
        if not best: continue
        c, off = best
        Wm = [f24(v) for v in d['world']]; Pm = [f24(v) for v in d['proj']]
        S = f24(d['vs'][2] * zs)
        def q(p):
            X, Y, Z = p[0] / 32768.0, p[1] / 32768.0, p[2] / 32768.0
            e = [X * Wm[j] + Y * Wm[3 + j] + Z * Wm[6 + j] + Wm[9 + j] for j in range(3)]
            cz = e[0] * Pm[2] + e[1] * Pm[6] + e[2] * Pm[10] + Pm[14]; cw = e[0] * Pm[3] + e[1] * Pm[7] + e[2] * Pm[11] + Pm[15]
            return cz / cw, cw
        rel, ge, ws = [], [], []
        for p in d['V']:
            pb = tuple(p[k] - off[k] for k in range(3)); qd, wd = q(p); qb, wb = q(pb)
            rel.append((qb - qd) / qb); ge.append(S * (qd - qb)); ws.append(wd)
        same = [tuple(p[k] - off[k] for k in range(3)) for p in d['V']] == list(c['V'])
        print('  decal b%d (tex %d) base b%d (tex %d) off %s nv %d same order %d: vs2 %.6g S %.6g; dq/q %.3g..%.3g; GE lead %.3g..%.3g; w %.0f..%.0f'
              % (d['i'], d['tex'], c['i'], c['tex'], off, len(d['V']), same, d['vs'][2], S, min(rel), max(rel), min(ge), max(ge), min(ws), max(ws)))
