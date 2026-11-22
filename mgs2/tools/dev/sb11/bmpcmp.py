# bmpcmp.py <dir a> <dir b>: the harness's shot_<vb>.bmp in both folders, pixel by pixel (numbers only): pixels that differ, the largest
# channel difference, how many differ by 1, 2, 3-4, 5-8, 9-16, 17+ levels, and the box that holds the differences
import sys, os, struct
def load(p):
    d = open(p, 'rb').read()
    off, = struct.unpack_from('<I', d, 10); w, h = struct.unpack_from('<ii', d, 18); bpp, = struct.unpack_from('<H', d, 28)
    row = ((w * bpp // 8) + 3) & ~3; px = []
    for y in range(abs(h)):
        r = d[off + row * y: off + row * y + w * bpp // 8]
        px.append([tuple(r[x * bpp // 8: x * bpp // 8 + 3]) for x in range(w)])
    return w, abs(h), px
A, B = sys.argv[1], sys.argv[2]
names = sorted(set(f for f in os.listdir(A) if f.startswith('shot_')) & set(f for f in os.listdir(B) if f.startswith('shot_')), key=lambda f: int(f[5:-4]))
for f in names:
    wa, ha, pa = load(os.path.join(A, f)); wb, hb, pb = load(os.path.join(B, f))
    if (wa, ha) != (wb, hb): print(f, 'sizes differ'); continue
    n = 0; mx = 0; hist = [0] * 6; box = [wa, ha, -1, -1]
    for y in range(ha):
        for x in range(wa):
            a, b = pa[y][x], pb[y][x]
            if a == b: continue
            dd = max(abs(a[i] - b[i]) for i in range(3)); n += 1; mx = max(mx, dd)
            hist[0 if dd == 1 else 1 if dd == 2 else 2 if dd <= 4 else 3 if dd <= 8 else 4 if dd <= 16 else 5] += 1
            box = [min(box[0], x), min(box[1], y), max(box[2], x), max(box[3], y)]
    print('%s: %d of %d pixels differ (%.2f%%), the largest by %d; by 1: %d, 2: %d, 3-4: %d, 5-8: %d, 9-16: %d, 17+: %d; box %s' %
          (f, n, wa * ha, 100.0 * n / (wa * ha), mx, *hist, box if n else '-'))
only = sorted(set(f for f in os.listdir(A) if f.startswith('shot_')) ^ set(f for f in os.listdir(B) if f.startswith('shot_')))
if only: print('in one folder only:', only)
