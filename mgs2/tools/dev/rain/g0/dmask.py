# dmask.py <dump a (.gz ok)> <dump b> <dyn a> <dyn b>: words that differ, split by the static gs mask (prof/gs_mask_v1.txt) and the dyn ranges
import sys, gzip, struct, bisect
def rd(p): return (gzip.open(p, 'rb') if p.endswith('.gz') else open(p, 'rb')).read()
a, b = rd(sys.argv[1]), rd(sys.argv[2])
def rng(p, tag):
    r = []
    for ln in open(p):
        f = ln.split()
        if len(f) == 3 and f[0] == tag: r.append((int(f[1], 16), int(f[2], 16)))
    return sorted(r)
st = rng('/mnt/project-files/mgs2/prof/gs_mask_v1.txt', 'mask'); da = rng(sys.argv[3], 'dyn'); db = rng(sys.argv[4], 'dyn')
def inr(r, w):
    for lo, hi in r:
        if lo <= w < hi: return True
    return False
sti = [x[0] for x in st]
def ins(w):
    i = bisect.bisect_right(sti, w) - 1
    return i >= 0 and st[i][0] <= w < st[i][1]
print('dyn ranges a %d b %d; only in a %d, only in b %d' % (len(da), len(db), len(set(da) - set(db)), len(set(db) - set(da))))
for x in sorted(set(da) ^ set(db))[:10]: print('  dyn diff', '%07x-%07x' % x, 'a' if x in set(da) else 'b')
n = min(len(a), len(b)); d = []
for o in range(0, n, 1 << 16):
    if a[o:o + (1 << 16)] != b[o:o + (1 << 16)]:
        for w in range(o, min(o + (1 << 16), n), 4):
            if a[w:w + 4] != b[w:w + 4]: d.append(w)
cls = {'static': [], 'dyn': [], 'unmasked': []}
for w in d:
    if ins(w): cls['static'].append(w)
    elif inr(da, w) or inr(db, w): cls['dyn'].append(w)
    else: cls['unmasked'].append(w)
print('differing words %d: static mask %d, dyn %d, unmasked %d' % (len(d), len(cls['static']), len(cls['dyn']), len(cls['unmasked'])))
for k in ('unmasked', 'dyn'):
    rs = []
    for w in cls[k]:
        if rs and w - rs[-1][1] <= 64: rs[-1][1] = w; rs[-1][2] += 1
        else: rs.append([w, w, 1])
    print(k, 'ranges', len(rs))
    for lo, hi, c in rs[:40]: print('  %07x-%07x %d words' % (lo, hi + 3, c))
for w in cls['unmasked'][:80]: print('  %07x %08x %08x' % (w, struct.unpack_from('<I', a, w)[0], struct.unpack_from('<I', b, w)[0]))
