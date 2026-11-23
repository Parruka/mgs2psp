import sys, struct
def rd(p):
    b = open(p, 'rb').read(); off = struct.unpack_from('<I', b, 10)[0]; w, h = struct.unpack_from('<ii', b, 18); bpp = struct.unpack_from('<H', b, 28)[0]
    st = (w * bpp // 8 + 3) & ~3; return b, off, w, abs(h), bpp // 8, st
def diff(a, c):
    A = rd(a); C = rd(c); s = set()
    b1, o1, w, h, n, st = A; b2, o2 = C[0], C[1]
    for y in range(h):
        r1 = b1[o1 + y * st: o1 + y * st + w * n]; r2 = b2[o2 + y * st: o2 + y * st + w * n]
        if r1 != r2:
            for x in range(w):
                p1 = r1[x * n:(x + 1) * n]; p2 = r2[x * n:(x + 1) * n]
                if p1 != p2: s.add((x, y, tuple(q - p for p, q in zip(p1, p2))))
    return s
f = sys.argv[1]; d = sys.argv[2:]
sets = {}
for i in range(len(d)):
    for j in range(i + 1, len(d)):
        s = diff(d[i] + '/' + f, d[j] + '/' + f); sets[(i, j)] = s
        ys = sorted(set(y for x, y, _ in s)); print(d[i], d[j], len(s), 'rows', ys[:1], ys[-1:], 'deltas', sorted(set(t for _, _, t in s))[:6])
k = list(sets)
if len(k) >= 2:
    p = [set((x, y) for x, y, _ in sets[q]) for q in k]
    print('positions: 01&02', len(p[0] & p[1]), '01&12', len(p[0] & p[2]) if len(p) > 2 else '', '02&12', len(p[1] & p[2]) if len(p) > 2 else '')
