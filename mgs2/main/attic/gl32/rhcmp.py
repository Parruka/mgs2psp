"""rhcmp.py A.log B.log (gl31 G2): the read-hash twin's 'rh: vb N <hash> <reads>' lines of two runs: per vblank the hash of every value
game code loaded (outside the draw pipeline) and the count of loads.  Prints how many vblanks differ in count and in value."""
import sys
def rd(p):
    d = {}
    for l in open(p, errors='replace'):
        if l.startswith('rh: vb '):
            w = l.split(); d[int(w[2])] = (w[3], int(w[4]))
    return d
a = rd(sys.argv[1]); b = rd(sys.argv[2])
vs = sorted(set(a) | set(b)); miss = [v for v in vs if v not in a or v not in b]
cnt = [v for v in vs if v in a and v in b and a[v][1] != b[v][1]]; val = [v for v in vs if v in a and v in b and a[v][1] == b[v][1] and a[v][0] != b[v][0]]
print('%s: %d vblanks (%s..%s); %s: %d; in one only %d; read counts differ in %d %s; counts equal but values differ in %d %s' % (
    sys.argv[1], len(a), min(a), max(a), sys.argv[2], len(b), len(miss), len(cnt), cnt[:10], len(val), val[:20]))
sys.exit(0 if not miss and not cnt and not val else 1)
