import sys, struct, numpy as np
# zw.py GER PSP EMU T decal...: for each decal, every earlier Z-writing df0 batch that overlaps it: overlap px, and the decal's model z minus
# that batch's (GE units) at the pixels where the PSP is lighter by > T and elsewhere (percentiles; counts only)
G = sys.argv[0].replace('p29/zw.py', 'margin.py'); args = sys.argv[1:]
sys.argv = [G] + args[:3]
src = open(G).read().split('for lo_i, hi_i in pairs:')[0]
exec(src)
T = float(args[3]); decals = [int(a) for a in args[4:]]
lit = (A > B + T)
Zs = {}
def zi(i):
    if i not in Zs: Zs[i] = zimg(i, {})
    return Zs[i]
for dk in decals:
    Zd = zi(dk); md = ~np.isnan(Zd); nl = int((md & lit).sum())
    print('decal b%d: %d px, lighter %d' % (dk, int(md.sum()), nl))
    for j in range(dk - 1, -1, -1):
        r = struct.unpack_from('<HBB4B6B2B4h4I', b, ob + 40 * j); flags, kind = r[2], r[14]
        if kind != 1 or not (flags & 32): continue
        Zj = zi(j); both = md & ~np.isnan(Zj)
        if both.sum() < 50: continue
        d = Zd - Zj; L = both & lit; O = both & ~lit
        def pc(v): return '-' if v.size == 0 else 'p5 %.1f p50 %.1f p95 %.1f' % tuple(np.percentile(v, [5, 50, 95]))
        print('  b%d (tex %d): overlap %d (lighter %d); decal - it: lighter %s | other %s; it above decal: %d px (lighter %d)' % (
            j, r[0], int(both.sum()), int(L.sum()), pc(d[L]), pc(d[O]), int((both & (d < 0)).sum()), int((L & (d < 0)).sum())))
