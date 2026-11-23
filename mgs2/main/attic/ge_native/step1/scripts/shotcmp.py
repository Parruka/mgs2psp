# shotcmp.py dirA dirB: the BMP shots both folders hold, compared by their pixel data (the bytes after the header's data offset):
# equal, or the count of differing pixels (4-byte or 3-byte as the header says) and the largest channel difference
import sys, os, struct
a, b = sys.argv[1], sys.argv[2]
names = sorted(set(os.listdir(a)) & set(os.listdir(b)))
eq = 0; dif = []
for n in names:
    if not n.endswith('.bmp'): continue
    x = open(os.path.join(a, n), 'rb').read(); y = open(os.path.join(b, n), 'rb').read()
    ox, oy = struct.unpack_from('<I', x, 10)[0], struct.unpack_from('<I', y, 10)[0]
    bpp = struct.unpack_from('<H', x, 28)[0] // 8
    px, py = x[ox:], y[oy:]
    if px == py: eq += 1; continue
    nd = 0; mx = 0
    for i in range(0, min(len(px), len(py)) - bpp + 1, bpp):
        if px[i:i + bpp] != py[i:i + bpp]:
            nd += 1; mx = max(mx, max(abs(px[i + k] - py[i + k]) for k in range(min(bpp, 3))))
    dif.append((n, nd, mx))
print('shots in both: %d; pixel data equal: %d; differing: %d' % (len([n for n in names if n.endswith('.bmp')]), eq, len(dif)))
for n, nd, mx in dif: print('  %s: %d pixels differ, largest channel difference %d' % (n, nd, mx))
print('only in A: %d, only in B: %d' % (len(set(os.listdir(a)) - set(os.listdir(b))), len(set(os.listdir(b)) - set(os.listdir(a)))))
