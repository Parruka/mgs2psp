import sys, struct
# pairchk.py GER d:b ...: why a decal and its base do (not) pair: vertex counts, strips, world equal, vertex sets with offset, triangle sets
fn = sys.argv[1]; prs = [tuple(map(int, a.split(':'))) for a in sys.argv[2:]]
b = open(fn, 'rb').read()
magic, vb, nb, nv, nt, nd, ob, ov, ot, od, w, h = struct.unpack_from('<12I', b, 0)
def get(i):
    r = struct.unpack_from('<HBB4B6B2B4h4I', b, ob + 40 * i); first = r[20]; xo = od + first
    x = struct.unpack_from('<36f', b, xo); nvx, nst = struct.unpack_from('<2I', b, xo + 164)
    V = [struct.unpack_from('<HHI4h', b, xo + 188 + 16 * k)[3:6] for k in range(nvx)]
    runs = [struct.unpack_from('<2H', b, xo + 188 + 16 * nvx + 4 * k) for k in range(nst)]
    T = set()
    for (f0, cnt) in runs:
        for k in range(f0 + 2, f0 + cnt):
            t = tuple(sorted((V[k - 2], V[k - 1], V[k])))
            if len(set(t)) == 3: T.add(t)
    return dict(x=x, V=V, runs=runs, T=T)
for d_, b_ in prs:
    D, B = get(d_), get(b_)
    off = (0, 5, 0)
    vd = set(D['V']); vb_ = set(tuple(p[k] + off[k] for k in range(3)) for p in B['V'])
    tb = set(tuple(sorted(tuple(p[k] + off[k] for k in range(3)) for p in t)) for t in B['T'])
    print('b%d/b%d: nv %d/%d, strips %d/%d, world equal %d, proj equal %d, vs/vo equal %d; distinct vertices %d/%d, decal set == base+off %d; triangles %d/%d, equal sets %d' % (
        d_, b_, len(D['V']), len(B['V']), len(D['runs']), len(B['runs']), D['x'][0:12] == B['x'][0:12], D['x'][12:28] == B['x'][12:28], D['x'][28:34] == B['x'][28:34],
        len(vd), len(vb_), vd == vb_, len(D['T']), len(tb), D['T'] == tb))
