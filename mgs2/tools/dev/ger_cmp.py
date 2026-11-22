#!/usr/bin/env python3
"""ger_cmp.py <a.ger> <b.ger> [max shown]: two GE replay files (ge/replay_fmt.h) of one vblank compared draw by draw (renderer_notes 11.13):
the live renderer's frame from the x86 check build (boot/xlive_chk.c MGS2_XLIVE_GER) against tools/gsref.c's export of the same vblank in
its live mode (GSREF_GELIVE).  Each file is read as a list of draws in batch order: a through batch gives one draw per GS primitive (its
vertices; strips and fans as their triangles), a transform batch one draw (its data bytes; a P06 pass, GERK_LIT: its GerLit, then the
call's GerXform, the vertices and the strips it names).  Draws are compared with their batch's state
(every field but first, count, px and twopass, the texture by what it holds: kind, format, size, texels and palette); twopass is counted
apart (a second pass that finds no failing pixel draws nothing).  Batch boundaries may differ (a primitive joins the batch before it when
the state is the same).  A draw that differs only in having a texture in the first file and none in the second (its flags' texture bit,
and the twopass that follows from the texture's lowest alpha), where the second file's batch says the PS2 wrote no pixels from it (px 0),
is counted apart as well: gsref takes a call's GE texture from the PS2's own textured primitives, so a call the PS2 drew nothing from
has none there, while the live renderer looks every texture up (renderer_notes 11.13).  Exit 0 when nothing else differs.
Game data: personal use only."""
import struct, sys, hashlib

BF = '<HBB4B6BBB4h4I'; BS = struct.calcsize(BF)
TF = '<4B4H4I'; TS = struct.calcsize(TF)
XF = '<12f16f3f3f2fI4f6I'; XS = struct.calcsize(XF); TS0 = struct.calcsize('<12f16f3f3f2fI')   # GerXform; TS0: where su sv tu tv lie
LF = '<3i5I4f'; LS = struct.calcsize(LF); LT0 = 32   # GerLit (offsets from itself: GerXform, vertices, strips; vtype stride nv nst pass); su sv tu tv at LT0
NAMES = ['tex', 'prim', 'flags', 'ztst', 'atst', 'aref', 'afail', 'ba', 'bb', 'bc', 'bd', 'fix', 'tfx', 'twopass', 'kind', 'sx0', 'sy0', 'sx1', 'sy1',
         'entry', 'first', 'count', 'px']

def load(fn):
    f = open(fn, 'rb').read()
    m, vb, nb, nv, nt, nd, ob, ov, ot, od, w, h = struct.unpack_from('<12I', f, 0); zs = struct.unpack_from('<f', f, 48)[0]
    assert m == 0x32524547, fn + ': not a GER2 file'
    B = [struct.unpack_from(BF, f, ob + i * BS) for i in range(nb)]
    V = [f[ov + 24 * i: ov + 24 * i + 24] for i in range(nv)]
    T = []
    for i in range(nt):
        t = struct.unpack_from(TF, f, ot + i * TS)
        kind, psm, rep, mina, tw, th, bw, rows, data, size, pal, npal = t
        if kind == 1:
            key = hashlib.md5(struct.pack('<4B4H', kind, psm, rep, mina, tw, th, bw, rows) + f[od + data: od + data + size] + f[od + pal: od + pal + 4 * npal]).hexdigest()[:16]
        else:
            key = 'fb%d.%d' % (kind, psm)
        T.append((key, t))
    return dict(vb=vb, zs=zs, B=B, V=V, T=T, D=f[od:od + nd], nd=nd)

def xsize(b, D):
    x = struct.unpack_from(XF, D, b[20])
    nv, nst, nidx, nfan, nfv, pad = x[-6:]
    if b[14] == 1: return XS + nv * 16 + nst * 4 + nidx * 2
    return XS + (nv + nfv) * 24 + ((nv + 3) & ~3) + nfan * 4

def litdata(b, D):   # a GERK_LIT draw's bytes: its GerLit, the call's GerXform, the vertices, the strips
    o = b[20]; xf, vtx, run, vtype, stride, nv, nst = struct.unpack_from(LF, D, o)[:7]
    return D[o: o + LS] + D[o + xf: o + xf + XS] + D[o + vtx: o + vtx + nv * stride] + D[o + run: o + run + nst * 4]

def same_but_ts(kind, a, b):   # two transform draws' bytes equal but for the texture scale and offset (which follow the texture)
    if kind == 'X': return a[:TS0] == b[:TS0] and a[TS0 + 16:] == b[TS0 + 16:]
    k = LS + TS0
    return a[:LT0] == b[:LT0] and a[LT0 + 16: k] == b[LT0 + 16: k] and a[k + 16:] == b[k + 16:]

def draws(r):
    out = []
    for i, b in enumerate(r['B']):
        st = list(b); tk = r['T'][b[0]][0] if b[0] != 0xffff else None
        st[0] = tk; tp = b[13]
        state = tuple(st[k] for k in range(len(st)) if NAMES[k] not in ('first', 'count', 'px', 'twopass'))
        if b[14] == 3:
            out.append(('L', state, tp, litdata(b, r['D']), i)); continue
        if b[14] != 0:
            n = xsize(b, r['D']); out.append(('X', state, tp, r['D'][b[20]: b[20] + n], i)); continue
        first, count, prim = b[20], b[21], b[1]
        V = r['V'][first: first + count]
        if prim == 0: ps = [(V[k],) for k in range(count)]
        elif prim == 1 or prim == 6: ps = [(V[k], V[k + 1]) for k in range(0, count - 1, 2)]
        elif prim == 2: ps = [(V[k], V[k + 1]) for k in range(count - 1)]
        elif prim == 3: ps = [(V[k], V[k + 1], V[k + 2]) for k in range(0, count - 2, 3)]
        elif prim == 4: ps = [(V[k], V[k + 1], V[k + 2]) for k in range(count - 2)]
        elif prim == 5: ps = [(V[0], V[k + 1], V[k + 2]) for k in range(count - 2)]
        else: ps = []
        for p in ps: out.append(('T', state, tp, b''.join(p), i))
    return out

def vstr(v):
    u, w, c, x, y, z = struct.unpack('<2fI3f', v); return 'u %.4f v %.4f c %08x x %.4f y %.4f z %.2f' % (u, w, c, x, y, z)

def main():
    a, b = load(sys.argv[1]), load(sys.argv[2]); mx = int(sys.argv[3]) if len(sys.argv) > 3 else 12
    print('%s: vb %d, %d batches, %d vertices, %d textures, %d data bytes, zscale %g' % (sys.argv[1], a['vb'], len(a['B']), len(a['V']), len(a['T']), a['nd'], a['zs']))
    print('%s: vb %d, %d batches, %d vertices, %d textures, %d data bytes, zscale %g' % (sys.argv[2], b['vb'], len(b['B']), len(b['V']), len(b['T']), b['nd'], b['zs']))
    da, db = draws(a), draws(b)
    print('draws: %d / %d (transform batches %d / %d, of them P06 passes %d / %d; through primitives %d / %d)' % (len(da), len(db),
          sum(d[0] != 'T' for d in da), sum(d[0] != 'T' for d in db), sum(d[0] == 'L' for d in da), sum(d[0] == 'L' for d in db),
          sum(d[0] == 'T' for d in da), sum(d[0] == 'T' for d in db)))
    n = min(len(da), len(db)); eq = 0; bad = 0; tpd = 0; shown = 0; kinds = {}; texonly = 0
    fl = [nm for nm in NAMES if nm not in ('first', 'count', 'px', 'twopass')]; FI = fl.index('flags')
    for k in range(n):
        x, y = da[k], db[k]
        if x[2] != y[2]: tpd += 1
        if x[0] == y[0] and x[1] == y[1] and x[3] == y[3]: eq += 1; continue
        if x[0] == y[0] and x[1][0] is not None and y[1][0] is None and b['B'][y[4]][22] == 0 and x[1][FI] == y[1][FI] | 2 and \
           all(x[1][j] == y[1][j] for j in range(1, len(fl)) if j != FI) and (x[0] == 'T' or same_but_ts(x[0], x[3], y[3])):
            texonly += 1; continue   # a transform draw's texture scale and offset (su sv tu tv) follow the texture
        bad += 1
        why = []
        if x[0] != y[0]: why.append('kind %s / %s' % (x[0], y[0]))
        if x[1] != y[1]:
            why.append('state ' + ','.join('%s %s/%s' % (fl[j], x[1][j], y[1][j]) for j in range(len(fl)) if x[1][j] != y[1][j]))
        if x[3] != y[3]: why.append('data')
        key = ' '.join(w.split(' ')[0] if not w.startswith('state') else 'state:' + ','.join(s.split(' ')[0] for s in w[6:].split(',')) for w in why)
        kinds[key] = kinds.get(key, 0) + 1
        if shown < mx:
            shown += 1
            print('draw %d (batches %d / %d): %s' % (k, x[4], y[4], '; '.join(why)))
            if x[0] == 'T' and y[0] == 'T' and x[3] != y[3]:
                va = [x[3][j:j + 24] for j in range(0, len(x[3]), 24)]; vb2 = [y[3][j:j + 24] for j in range(0, len(y[3]), 24)]
                for j in range(min(len(va), len(vb2))):
                    if va[j] != vb2[j]: print('   vertex %d: %s\n             %s' % (j, vstr(va[j]), vstr(vb2[j])))
    print('compared %d draws: %d equal, %d with a texture only in the first file (the PS2 wrote no pixels from them), %d differ otherwise; twopass differs on %d; only in the first file %d, only in the second %d' % (n, eq, texonly, bad, tpd, len(da) - n, len(db) - n))
    for k2, v in sorted(kinds.items(), key=lambda z: -z[1]): print('  %6d  %s' % (v, k2))
    return 0 if bad == 0 and len(da) == len(db) else 1

if __name__ == '__main__':
    sys.exit(main())
