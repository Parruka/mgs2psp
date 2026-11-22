# bmp3.py <x.bmp> <on.bmp> <off.bmp>: where x differs from on: how many of those pixels also differ between on and off (the people's
# pixels), how many equal off there, and the bounding box of x's differences (y counted from the top); same for on vs off.
import sys, struct
def rd(p):
    d = open(p, 'rb').read(); off = struct.unpack_from('<I', d, 10)[0]; w, h = struct.unpack_from('<ii', d, 18); bpp = struct.unpack_from('<H', d, 28)[0]
    return d, off, w, h, bpp
X, ox, w, h, bpp = rd(sys.argv[1]); A, oa, _, _, _ = rd(sys.argv[2]); B, ob, _, _, _ = rd(sys.argv[3])
bp = bpp // 8; row = (w * bp + 3) & ~3; H = abs(h)
def px(d, o, x, y): i = o + y * row + x * bp; return d[i:i + 3]
xs = []; ab = 0; inab = 0; eqoff = 0; between = 0
for y in range(H):
    for x in range(w):
        a = px(A, oa, x, y); b = px(B, ob, x, y); v = px(X, ox, x, y)
        if a != b: ab += 1
        if v != a:
            yt = (H - 1 - y) if h > 0 else y
            xs.append((x, yt))
            if a != b:
                inab += 1
                if v == b: eqoff += 1
                elif all(min(a[c], b[c]) <= v[c] <= max(a[c], b[c]) for c in range(3)): between += 1
print('on vs off: %d pixels; x vs on: %d pixels, %d of them where on and off differ (%d equal off, %d between on and off)' % (ab, len(xs), inab, eqoff, between))
if xs: print('box x %d-%d, y %d-%d' % (min(p[0] for p in xs), max(p[0] for p in xs), min(p[1] for p in xs), max(p[1] for p in xs)))
