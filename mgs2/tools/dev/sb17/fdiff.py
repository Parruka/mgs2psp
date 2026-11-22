# fdiff.py <log A> <log B> [<log C>]: frames ("picture: vb N buffer B list L frame F") that differ between A and B; with C, also whether
# that set equals the set where B and C differ
import re, sys
def fr(p):
    d = {}
    for l in open(p, errors='replace'):
        m = re.match(r'picture: vb (\d+) buffer \d list (\w+) frame (\w+)', l)
        if m: d[int(m.group(1))] = (m.group(2), m.group(3))
    return d
a, b = fr(sys.argv[1]), fr(sys.argv[2])
assert a.keys() == b.keys(), (len(a), len(b))
dab = sorted(v for v in a if a[v] != b[v])
print('frames %d, differ %d%s' % (len(a), len(dab), (', first %d last %d' % (dab[0], dab[-1])) if dab else ''))
if len(sys.argv) > 3:
    c = fr(sys.argv[3]); dbc = sorted(v for v in b if b[v] != c[v])
    print('B vs C differ %d; the same frames: %s; only A/B %d, only B/C %d' % (len(dbc), set(dab) == set(dbc), len(set(dab) - set(dbc)), len(set(dbc) - set(dab))))
