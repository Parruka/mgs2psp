# kbdiff.py <C2-on log> <C2-off log>: per frame the frame data KB with C2 on minus off: totals, the largest rise and fall, how many rise
import sys, re
R = re.compile(r'^xlive gb: vb (\d+): .*?, (\d+) KB data,')
def rd(p):
    d = {}
    for l in open(p, errors='replace'):
        m = R.match(l)
        if m: d[int(m.group(1))] = int(m.group(2))
    return d
a, r = rd(sys.argv[1]), rd(sys.argv[2])
ks = sorted(set(a) & set(r)); dd = [(a[k] - r[k], k) for k in ks]
ch = [x for x in dd if x[0] != 0]
print('frames %d, changed %d; KB on %d, off %d (%.2f%%); rise in %d frames (largest +%d KB at %d), fall in %d (largest %d KB at %d); largest frame on %d KB (vb %d), off %d KB (vb %d)' % (
    len(ks), len(ch), sum(a[k] for k in ks), sum(r[k] for k in ks), 100.0 * (sum(a[k] for k in ks) / max(1, sum(r[k] for k in ks)) - 1),
    sum(1 for x in ch if x[0] > 0), max(ch)[0], max(ch)[1], sum(1 for x in ch if x[0] < 0), min(ch)[0], min(ch)[1],
    max((a[k], k) for k in ks)[0], max((a[k], k) for k in ks)[1], max((r[k], k) for k in ks)[0], max((r[k], k) for k in ks)[1]))
