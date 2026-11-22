import sys, struct, collections, re
# cmp.py A.bin B.bin GERDIR (gl30 G1 off the twin, 2026-11-20): gerl's records of the same frames (A: gl29, B: gl30), batch by batch.  A
# batch's tokens are its list words without BASE, each address word as (command, region, offset; for aux the offset from the batch's own
# aux bytes), and its aux bytes.  Prints, per frame, the batches that differ; checks that each is a 0x42 XFORM decal with the depth test
# that A drew O (its vertices in the data block) and B drew from aux, that the two differ only in their address words, and that B's aux
# copy is the decal's own vertices with each position moved by one offset (reported).
def recs(p):
    b = open(p, 'rb').read(); o = 0
    while o < len(b):
        vb, nm, n, alow, cap = struct.unpack_from('<5I', b, o); o += 20
        mk = [struct.unpack_from('<4I', b, o + 16 * k) for k in range(nm)]; o += 16 * nm
        w = struct.unpack_from('<%dI' % n, b, o); o += 4 * n
        aux = b[o:o + cap - alow]; o += cap - alow
        yield vb, mk, w, alow, cap, aux
def batches(mk, w, alow0, cap, aux):
    """{batch: (tokens, aux bytes, address tokens)}; the region state runs through the list"""
    first = collections.OrderedDict()
    for (i, ps, n, al) in mk:
        if i not in first: first[i] = (n, al)
    keys = list(first.keys())
    out = {}; reg = None
    # the words before batch 0 (the set-up): region state only
    n0 = first[keys[0]][0]
    for x in w[:n0]:
        if x >> 24 == 0x10: reg = (x >> 16) & 0xf
    for a, i in enumerate(keys[:-1]):
        n_s, al_s = first[i]; n_e, al_e = first[keys[a + 1]]
        toks = []; adr = []
        for x in w[n_s:n_e]:
            c = x >> 24
            if c == 0x10: reg = (x >> 16) & 0xf; continue
            if c in (0x01, 0x02):
                off = x & 0xffffff; t = (c, reg, off - al_e if reg == 2 else off); toks.append(('A',) + t); adr.append(t); continue
            toks.append(x)
        ab = aux[al_e - alow0:al_s - alow0]
        out[i] = (toks, ab, adr)
    return out
def ger(fn):
    b = open(fn, 'rb').read()
    magic, vb, nb, nv, nt, nd, ob, ov, ot, od = struct.unpack_from('<10I', b, 0)
    def bat(i):
        r = struct.unpack_from('<HBB4B6B2B4h4I', b, ob + 40 * i)
        return dict(flags=r[2], kind=r[14], sig=(r[7] | r[8] << 2 | r[9] << 4 | r[10] << 6) & 0xff, first=r[20])
    def verts(first):
        xo = od + first; nvx, nst = struct.unpack_from('<2I', b, xo + 164)
        return [struct.unpack_from('<HHI4h', b, xo + 188 + 16 * k) for k in range(nvx)], xo + 188
    return bat, verts, od
A = recs(sys.argv[1]); B = recs(sys.argv[2]); G = sys.argv[3]
XO = open(sys.argv[4], 'w') if len(sys.argv) > 4 else None   # (optional) the differing batches: 'vb batch offset ok'
tot = collections.Counter(); offs = collections.Counter(); bad = []
for (ra, rb) in zip(A, B):
    vb = ra[0]; assert vb == rb[0]
    ba = batches(ra[1], ra[2], ra[3], ra[4], ra[5]); bb = batches(rb[1], rb[2], rb[3], rb[4], rb[5])
    if set(ba) != set(bb): bad.append((vb, 'batch sets differ')); continue
    dif = [i for i in ba if ba[i][0] != bb[i][0] or ba[i][1] != bb[i][1]]
    tot['frames'] += 1
    if not dif: tot['frames equal'] += 1; continue
    tot['frames with differing batches'] += 1; tot['differing batches'] += len(dif)
    bat, verts, od = ger('%s/f%d.ger' % (G, vb))
    for i in dif:
        g = bat(i)
        ok42 = g['kind'] == 1 and g['flags'] & 16 and g['flags'] & 4 and g['sig'] == 0x42
        ta, tb = ba[i][0], bb[i][0]
        # the same words where neither is an address
        strip = lambda t: [x if not isinstance(x, tuple) else 'ADDR' for x in t]
        same_rest = strip(ta) == strip(tb)
        a_data = all(t[1] == 3 for t in ba[i][2]) and len(ba[i][1]) == 0
        b_aux = len(bb[i][2]) > 0 and all(t[1] == 2 for t in bb[i][2])
        V, vo = verts(g['first'])
        # B's copy: the decal's vertices, each position moved by one offset
        ab = bb[i][1]; nvx = len(V); copy_ok = None; off = None
        if len(ab) >= 16 * nvx:
            C = [struct.unpack_from('<HHI4h', ab, 16 * k) for k in range(nvx)]
            o = set((V[k][3] - C[k][3], V[k][4] - C[k][4], V[k][5] - C[k][5]) for k in range(nvx))
            copy_ok = len(o) == 1 and all(V[k][0:3] == C[k][0:3] and V[k][6] == C[k][6] for k in range(nvx)) and not any(ab[16 * nvx:])
            off = o.pop() if len(o) == 1 else None
        # A's address words: the decal's own data; B's: the copy at the same vertex positions
        a_rel = [t[2] - (vo - od) for t in ba[i][2]]; b_rel = [t[2] for t in bb[i][2]]
        same_pos = a_rel == b_rel
        good = ok42 and same_rest and a_data and b_aux and copy_ok and same_pos
        tot['differing batches as expected' if good else 'differing batches NOT as expected'] += 1
        offs[off] += 1
        if XO: XO.write('%d %d %s %d\n' % (vb, i, ','.join(map(str, off)) if off else '-', int(bool(good))))
        if not good and len(bad) < 20: bad.append((vb, i, dict(ok42=bool(ok42), same_rest=same_rest, a_data=a_data, b_aux=b_aux, copy_ok=copy_ok, same_pos=same_pos)))
for k in sorted(tot): print('%-40s %d' % (k, tot[k]))
print('offsets of the copies:', dict(offs))
for x in bad: print('BAD', x)
