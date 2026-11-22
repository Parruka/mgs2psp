# shotcmp.py <psp.bmp> <master.bmp> <c2on.bmp> <c2off.bmp>: where the PSP shot differs from its build's master shot: pixels, % of the
# picture, largest channel difference, share by 1 level, box (y from the top), pixels in the bottom 16 rows, and how many lie where the
# C2-on and C2-off master pictures differ (the people's pixels drawn differently by C2)
import sys, struct
def rd(p):
    d = open(p, 'rb').read(); off = struct.unpack_from('<I', d, 10)[0]; w, h = struct.unpack_from('<ii', d, 18); bpp = struct.unpack_from('<H', d, 28)[0]
    return d, off, w, h, bpp
X, ox, w, h, bpp = rd(sys.argv[1]); M, om, _, _, _ = rd(sys.argv[2]); A, oa, _, _, _ = rd(sys.argv[3]); B, ob, _, _, _ = rd(sys.argv[4])
bp = bpp // 8; row = (w * bp + 3) & ~3; H = abs(h)
n = mx = one = bot = ppl = ab = 0; xs = []; ys = []
for y in range(H):
    yt = (H - 1 - y) if h > 0 else y
    for x in range(w):
        i = y * row + x * bp
        v, m, a, b = X[ox + i:ox + i + 3], M[om + i:om + i + 3], A[oa + i:oa + i + 3], B[ob + i:ob + i + 3]
        if a != b: ab += 1
        if v != m:
            n += 1; d = max(abs(v[c] - m[c]) for c in range(3)); mx = max(mx, d); one += d == 1
            bot += yt >= H - 16; ppl += a != b; xs.append(x); ys.append(yt)
name = sys.argv[1].split('/')[-1]
if not n: print('%s: equal' % name); sys.exit()
print('%s: %d px (%.2f%%), largest %d, by 1 level %.1f%%, box x %d-%d y %d-%d, bottom 16 rows %d, on people pixels %d (of %d where C2 on/off differ)'
      % (name, n, 100.0 * n / (w * H), mx, 100.0 * one / n, min(xs), max(xs), min(ys), max(ys), bot, ppl, ab))
