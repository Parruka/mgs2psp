# shotdiff.py <dirA> <dirB> <vb...>: per shot, pixels that differ (any channel), the largest channel difference, the rows they lie in
import sys, struct
def rd(p):
    d = open(p, 'rb').read(); off = struct.unpack_from('<I', d, 10)[0]; w, h = struct.unpack_from('<ii', d, 18); bpp = struct.unpack_from('<H', d, 28)[0]
    return d, off, w, h, bpp // 8
A, B = sys.argv[1], sys.argv[2]
for vb in sys.argv[3:]:
    try: X, ox, w, h, bp = rd('%s/shot_%s.bmp' % (A, vb)); Y, oy, w2, h2, bp2 = rd('%s/shot_%s.bmp' % (B, vb))
    except OSError as e: print(vb, 'missing', e); continue
    assert (w, h, bp) == (w2, h2, bp2)
    row = (w * bp + 3) & ~3; H = abs(h); n = mx = 0; rlo = 10**9; rhi = -1
    for y in range(H):
        yt = (H - 1 - y) if h > 0 else y
        a = X[ox + y * row: ox + y * row + w * bp]; b = Y[oy + y * row: oy + y * row + w * bp]
        if a == b: continue
        for x in range(w):
            p, q = a[x * bp: x * bp + 3], b[x * bp: x * bp + 3]
            if p != q:
                n += 1; mx = max(mx, max(abs(p[i] - q[i]) for i in range(3))); rlo = min(rlo, yt); rhi = max(rhi, yt)
    print('%6s: %6d px differ (%.2f%%), largest %3d%s' % (vb, n, 100.0 * n / (w * H), mx, ', rows %d-%d' % (rlo, rhi) if n else ''))
