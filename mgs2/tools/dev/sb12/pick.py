# pick.py <C2-on log> <off log>: per frame the data KB C2 saves (a proxy for the P06 vertices drawn) and the P06/P07 calls; the frames
# with the most, by part of the codec route
import sys, re
R = re.compile(r'^xlive gb: vb (\d+): .*?, (\d+) KB data,.*?P06 and P07 calls (\d+),')
def rd(p):
    d = {}
    for l in open(p, errors='replace'):
        m = R.match(l)
        if m: d[int(m.group(1))] = (int(m.group(2)), int(m.group(3)))
    return d
a, r = rd(sys.argv[1]), rd(sys.argv[2])
sv = {v: (r[v][0] - a[v][0], a[v][1]) for v in a if v in r and r[v][0] != a[v][0]}
parts = [(0, 15600), (15601, 19900), (19901, 26000), (26001, 28800), (28801, 31800), (31801, 37800)]
for lo, hi in parts:
    fr = sorted([v for v in sv if lo <= v <= hi], key=lambda v: -sv[v][0])
    print('vb %d-%d: %d frames with C2; top:' % (lo, hi, len(fr)), ', '.join('%d (%d KB, %d calls)' % (v, sv[v][0], sv[v][1]) for v in fr[:12]))
print('by 500-vb bucket: the frame with the most')
b = {}
for v in sv:
    k = v // 500
    if k not in b or sv[v][0] > sv[b[k]][0]: b[k] = v
print(', '.join('%d (%d KB, %d c)' % (b[k], sv[b[k]][0], sv[b[k]][1]) for k in sorted(b)))
