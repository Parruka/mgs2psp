import sys, struct, collections
# subana.py GER...: gl30 design check (2026-11-20, build agent). For every 0x42 XFORM decal with the depth test (as gelist.c dz_batch's
# targets): the candidates as gl29's dz_pair walks them (up to 64 back, XFORM, Z write, not 0x42, same 34 matrix words), how many pass
# the cheap reject (a vertex within 64 on each axis of the decal's first vertex), the exact base (gl29's rule), and for the subset rule
# every valid offset of the nearest subset base (to see whether "one offset" is ever ambiguous).
def load(fn):
    b = open(fn, 'rb').read()
    magic, vb, nb, nv, nt, nd, ob, ov, ot, od, w, h = struct.unpack_from('<12I', b, 0)
    B = []
    for i in range(nb):
        r = struct.unpack_from('<HBB4B6B2B4h4I', b, ob + 40 * i)
        flags, kind = r[2], r[14]; sig = (r[7] | r[8] << 2 | r[9] << 4 | r[10] << 6) & 0xff
        e = dict(i=i, flags=flags, kind=kind, sig=sig, tex=r[0])
        if kind == 1:
            xo = od + r[20]; xw = struct.unpack_from('<34I', b, xo); nvx, nst = struct.unpack_from('<2I', b, xo + 164)
            V = [struct.unpack_from('<HHI4h', b, xo + 188 + 16 * k)[3:6] for k in range(nvx)]
            runs = [struct.unpack_from('<2H', b, xo + 188 + 16 * nvx + 4 * k) for k in range(nst)]
            T = set()
            for (f0, cnt) in runs:
                for k in range(f0 + 2, f0 + cnt):
                    t = tuple(sorted((V[k - 2], V[k - 1], V[k])))
                    if len(set(t)) == 3: T.add(t)
            e.update(xw=xw, V=V, S=set(V), T=T, nst=nst)
        B.append(e)
    return vb, B
def sh(p, o): return (p[0] - o[0], p[1] - o[1], p[2] - o[2])
tot = collections.Counter(); amb = []; npass = collections.Counter(); nbv = collections.Counter()
for fn in sys.argv[1:]:
    vb, B = load(fn)
    for d in B:
        if d['kind'] != 1 or not (d['flags'] & 16) or not (d['flags'] & 4) or d['sig'] != 0x42: continue
        tot['targets'] += 1
        cands = [c for c in B[max(0, d['i'] - 64):d['i']] if c['kind'] == 1 and (c['flags'] & 32) and not ((c['flags'] & 4) and c['sig'] == 0x42)
                 and c['xw'][9:12] == d['xw'][9:12]]
        cands.reverse()
        v0 = d['V'][0]; exact = None; sub = None; npc = 0
        for c in cands:
            if c['xw'] != d['xw']: continue
            if not any(max(abs(v0[0] - q[0]), abs(v0[1] - q[1]), abs(v0[2] - q[2])) <= 64 for q in c['V']): continue
            npc += 1; nbv[len(c['V'])] += 1
            dmin = min(d['S'])
            bs = sorted(c['S'])
            if len(c['S']) == len(d['S']):
                o = sh(dmin, bs[0])
                if max(map(abs, o)) <= 64 and set(sh(p, o) for p in d['S']) == c['S']:
                    if set(tuple(sorted(sh(p, o) for p in t)) for t in d['T']) == c['T']: exact = (c['i'], o); break
            if sub is None:
                offs = []
                for q in bs:
                    o = sh(dmin, q)
                    if max(map(abs, o)) > 64: continue
                    if set(sh(p, o) for p in d['S']) <= c['S'] and set(tuple(sorted(sh(p, o) for p in t)) for t in d['T']) <= c['T']: offs.append(o)
                if offs: sub = (c['i'], offs)
        npass[npc] += 1
        if exact: tot['exact'] += 1; continue
        if sub:
            tot['subset'] += 1; tot['sub offsets %d' % len(sub[1])] += 1
            for o in sub[1]: tot['sub off %s' % (o,)] += 1
            tot['sub gap %d' % (d['i'] - sub[0])] += 1
            if len(sub[1]) > 1 and len(amb) < 10: amb.append((vb, d['i'], sub))
            continue
        tot['other'] += 1
for k in sorted(tot): print(k, tot[k])
print('candidates past the cheap reject per decal:', sorted(npass.items()))
print('their vertex counts:', sorted(nbv.items()))
for a in amb: print('ambiguous', a)
