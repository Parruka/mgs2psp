#!/usr/bin/env python3
"""picdiff.py DIR_A DIR_B [PNGDIR] (python3 -I): shot_*.bmp pairs of two emulator runs, image data only: pixels that differ, their box, a
12 x 8 map of the share that differs.  PNGDIR: A, B and |A-B| x4 side by side as PNG, for a look inside the container only."""
import sys, os, struct, zlib
def bmp(p):
    d = open(p, 'rb').read(); off, = struct.unpack_from('<I', d, 10); w, h, pl, bpp = struct.unpack_from('<iiHH', d, 18)
    top = h < 0; h = abs(h); bs = bpp // 8; row = (w * bs + 3) & ~3; R = []
    for y in range(h):
        yy = y if top else h - 1 - y; r = d[off + yy * row: off + yy * row + w * bs]
        R.append(bytes(c for x in range(w) for c in (r[x * bs + 2], r[x * bs + 1], r[x * bs])))
    return w, h, R
def png(p, w, h, rows):
    raw = b''.join(b'\0' + r for r in rows)
    def ch(t, d): return struct.pack('>I', len(d)) + t + d + struct.pack('>I', zlib.crc32(t + d) & 0xffffffff)
    open(p, 'wb').write(b'\x89PNG\r\n\x1a\n' + ch(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 2, 0, 0, 0)) + ch(b'IDAT', zlib.compress(raw, 6)) + ch(b'IEND', b''))
A, B = sys.argv[1], sys.argv[2]; out = sys.argv[3] if len(sys.argv) > 3 else None
for f in sorted(os.listdir(A), key=lambda s: int(''.join(c for c in s if c.isdigit()) or 0)):
    if not f.startswith('shot_') or not os.path.exists(B + '/' + f): continue
    w, h, a = bmp(A + '/' + f); w2, h2, b = bmp(B + '/' + f)
    if (w, h) != (w2, h2): print(f, 'sizes differ'); continue
    n = 0; x0 = w; x1 = -1; y0 = h; y1 = -1; G = [[0] * 12 for _ in range(8)]; D = []; sa = 0
    for y in range(h):
        ra, rb = a[y], b[y]; dr = bytearray(w * 3)
        if ra != rb:
            for x in range(w):
                pa = ra[3 * x:3 * x + 3]; pb = rb[3 * x:3 * x + 3]
                if pa != pb:
                    n += 1; x0 = min(x0, x); x1 = max(x1, x); y0 = min(y0, y); y1 = max(y1, y); G[y * 8 // h][x * 12 // w] += 1
                    for j in range(3): v = abs(pa[j] - pb[j]); sa += v; dr[3 * x + j] = min(255, 4 * v)
        D.append(bytes(dr))
    cell = (w // 12) * (h // 8)
    print('%s: %dx%d, %d pixels differ (%.1f%%), box x %d-%d y %d-%d, mean |d| of those %.1f' % (f, w, h, n, 100.0 * n / (w * h), x0, x1, y0, y1, sa / (3.0 * n) if n else 0))
    for r in G: print('    ' + ' '.join('%3d' % min(999, 100 * c // cell) for c in r))
    if out:
        os.makedirs(out, exist_ok=True); png(out + '/' + f[:-4] + '.png', 3 * w, h, [a[y] + b[y] + D[y] for y in range(h)])
