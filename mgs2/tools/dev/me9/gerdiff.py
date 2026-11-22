# gerdiff.py <a.ger> <b.ger>: where two replay files of one frame (ge/replay_fmt.h) differ: header, batches (fields), the
# through-vertices (by batch), the textures (fields, texels, palette) and the data block (by the batches that point into it).
# me9 (task #130): the late-picture runs' differing frames.  Game data: personal use only; prints counts and offsets only.
import struct, sys
HB = '<12I3f'; BS = '<HBB4B6B2B4h4I'; VS = 24; TS = '<4B4H4I'
def rd(p):
    f = open(p, 'rb').read()
    h = struct.unpack_from(HB, f, 0); (magic, vb, nb, nv, nt, nd, ob, ov, ot, od, w, hh) = h[:12]
    B = [struct.unpack_from(BS, f, ob + i * struct.calcsize(BS)) for i in range(nb)]
    V = [f[ov + i * VS: ov + (i + 1) * VS] for i in range(nv)]
    T = [struct.unpack_from(TS, f, ot + i * struct.calcsize(TS)) for i in range(nt)]
    D = f[od: od + nd]
    return h, B, V, T, D
a, b = rd(sys.argv[1]), rd(sys.argv[2])
ha, Ba, Va, Ta, Da = a; hb, Bb, Vb, Tb, Db = b
print('header', 'equal' if ha == hb else 'DIFFERS %s / %s' % (ha, hb))
print('batches', len(Ba), len(Bb), 'differing', sum(1 for x, y in zip(Ba, Bb) if x != y))
for i, (x, y) in enumerate(zip(Ba, Bb)):
    if x != y: print('  batch', i, x, '/', y); break
# through vertices: which batches own the differing ones
nvd = [i for i, (x, y) in enumerate(zip(Va, Vb)) if x != y]
print('through-vertices', len(Va), len(Vb), 'differing', len(nvd))
if nvd:
    bmap = []
    for i, x in enumerate(Ba):
        kind = x[14]; first, count = x[20], x[21]
        if kind == 0: bmap.append((first, first + count, i, x[19]))
    hits = {}
    for v in nvd:
        for lo, hi, i, ent in bmap:
            if lo <= v < hi: hits.setdefault((i, ent), []).append(v); break
    for (i, ent), vs in list(hits.items())[:12]:
        x = struct.unpack('<2fI3f', Va[vs[0]]); y = struct.unpack('<2fI3f', Vb[vs[0]])
        print('  batch %d entry %06x: %d vertices differ, first %d: %s / %s' % (i, ent, len(vs), vs[0], x, y))
tdiff = []
for i, (x, y) in enumerate(zip(Ta, Tb)):
    fx = x != y
    da = Da[x[8]: x[8] + x[9]]; db = Db[y[8]: y[8] + y[9]]
    pa = Da[x[10]: x[10] + 4 * x[11]]; pb = Db[y[10]: y[10] + 4 * y[11]]
    if fx or da != db or pa != pb:
        nb_ = sum(1 for p, q in zip(da, db) if p != q); np_ = sum(1 for p, q in zip(pa, pb) if p != q)
        tdiff.append((i, x[:8], fx, nb_, len(da), np_, len(pa)))
print('textures', len(Ta), len(Tb), 'differing', len(tdiff))
for t in tdiff[:12]: print('  texture %d kind/psm/rep/minA %s fields %s, texel bytes differing %d of %d, palette bytes %d of %d' % (t[0], t[1][:4], 'differ' if t[2] else 'equal', t[3], t[4], t[5], t[6]))
print('data block', len(Da), len(Db), 'equal' if Da == Db else 'differs: %d bytes' % sum(1 for p, q in zip(Da, Db) if p != q))
if Da != Db and len(Da) == len(Db):
    tex_ranges = []
    for x in Ta: tex_ranges += [(x[8], x[8] + x[9], 'texels'), (x[10], x[10] + 4 * x[11], 'palette')]
    other = 0; first = None
    for k in range(len(Da)):
        if Da[k] != Db[k] and not any(lo <= k < hi for lo, hi, _ in tex_ranges):
            other += 1
            if first is None: first = k
    print('  data bytes differing outside textures and palettes: %d (first at %s)' % (other, first))
    if first is not None:
        best = None
        for i, x in enumerate(Ba):
            if x[14] != 0 and x[20] <= first and (best is None or x[20] > Ba[best][20]): best = i
        if best is not None: print('  nearest batch before it: %d kind %d entry %06x first %d' % (best, Ba[best][14], Ba[best][19], Ba[best][20]))
