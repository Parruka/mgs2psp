import sys, struct, collections
# fbana.py GER...: the 0x42 decals that X cannot pair (vertex set): subset of one base, union of bases, or other
def load(fn):
    b = open(fn, 'rb').read()
    magic, vb, nb, nv, nt, nd, ob, ov, ot, od, w, h = struct.unpack_from('<12I', b, 0)
    B = []
    for i in range(nb):
        r = struct.unpack_from('<HBB4B6B2B4h4I', b, ob + 40 * i)
        flags, kind = r[2], r[14]; sig = (r[7] | r[8] << 2 | r[9] << 4 | r[10] << 6) & 0xff
        e = dict(i=i, flags=flags, kind=kind, sig=sig, tex=r[0])
        if kind == 1:
            xo = od + r[20]; x = struct.unpack_from('<36f', b, xo); nvx, nst = struct.unpack_from('<2I', b, xo + 164)
            V = [struct.unpack_from('<HHI4h', b, xo + 188 + 16 * k)[3:6] for k in range(nvx)]
            runs = [struct.unpack_from('<2H', b, xo + 188 + 16 * nvx + 4 * k) for k in range(nst)]
            T = set()
            for (f0, cnt) in runs:
                for k in range(f0 + 2, f0 + cnt):
                    t = tuple(sorted((V[k - 2], V[k - 1], V[k])))
                    if len(set(t)) == 3: T.add(t)
            e.update(x=x[0:34], V=V, S=set(V), T=T, nst=nst)
        B.append(e)
    return vb, B
def sh(p, o): return (p[0] - o[0], p[1] - o[1], p[2] - o[2])
tot = collections.Counter(); ex = []
for fn in sys.argv[1:]:
    vb, B = load(fn)
    for d in B:
        if d['kind'] != 1 or not (d['flags'] & 16) or not (d['flags'] & 4) or d['sig'] != 0x42: continue
        cands = [c for c in B[max(0, d['i'] - 64):d['i']] if c['kind'] == 1 and (c['flags'] & 32) and not ((c['flags'] & 4) and c['sig'] == 0x42) and c['x'] == d['x']]
        cands.reverse()
        exact = None; sub = None
        for c in cands:
            # offsets that map some decal vertex onto a base vertex, |o| <= 64
            cnt = collections.Counter()
            for p in list(d['S'])[:8]:
                for q in c['S']:
                    o = sh(p, q)
                    if max(abs(o[0]), abs(o[1]), abs(o[2])) <= 64: cnt[o] += 1
            for o, _ in cnt.most_common(3):
                ds = set(sh(p, o) for p in d['S'])
                dt = set(tuple(sorted(sh(p, o) for p in t)) for t in d['T'])
                if ds == c['S'] and dt == c['T']: exact = (c['i'], o); break
                if ds <= c['S'] and dt <= c['T'] and sub is None: sub = (c['i'], o, len(ds), len(c['S']))
            if exact: break
        if exact: tot['pair'] += 1; continue
        if sub: tot["subset of one base"] += 1; tot["sub off %s" % (sub[1],)] += 1; tot["sub base gap %d" % (d["i"] - sub[0])] += 0; continue
        # union over all same-matrix candidates with offset (0,5,0) and the most common offset
        allS = set(); allT = set()
        for c in cands: allS |= c['S']; allT |= c['T']
        best = None
        for o in [(0, 5, 0), (0, -5, 0)]:
            ds = set(sh(p, o) for p in d['S']); dt = set(tuple(sorted(sh(p, o) for p in t)) for t in d['T'])
            m = (len(ds & allS), len(ds), len(dt & allT), len(dt))
            if best is None or m[0] > best[1][0]: best = (o, m)
        if best[1][0] == best[1][1] and best[1][2] == best[1][3]: tot['union of bases'] += 1; continue
        k = 'same-matrix candidates %d, vertices found %d/%d, triangles %d/%d' % (len(cands), *best[1])
        tot['other'] += 1
        if len(ex) < 12: ex.append((vb, d['i'], d['tex'], len(d['V']), k))
print(dict(tot))
for e in ex: print('vb %d b%d tex %d nv %d: %s' % e)
