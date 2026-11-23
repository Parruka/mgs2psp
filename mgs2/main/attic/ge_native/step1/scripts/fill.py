# fill.py log(.gz) [window]: the 'nge f' fill lines by vblank windows: pictures, peak used KB, cooks, re-cooks, evictions, no room, groups native / today
import sys, gzip
w = int(sys.argv[2]) if len(sys.argv) > 2 else 1000
agg = {}
tot = [0] * 8; pk = 0
for l in (gzip.open(sys.argv[1], 'rt', errors='replace') if sys.argv[1].endswith('.gz') else open(sys.argv[1], errors='replace')):
    if not l.startswith('nge f vb '): continue
    t = l.split(); d = dict(zip(t[2::2], map(int, t[3::2])))
    k = d['vb'] // w * w; a = agg.setdefault(k, [0, 0, 0, 0, 0, 0, 0, 0])
    a[0] += 1; a[1] = max(a[1], d['peak']); a[2] += d['ck']; a[3] += d['rc']; a[4] += d['ev']; a[5] += d['nr']; a[6] += d['nat']; a[7] += d['tod']
print('%-7s %5s %8s %6s %6s %6s %7s %9s %8s' % ('vb', 'pics', 'peakKB', 'cook', 'recook', 'evict', 'noroom', 'grp nat', 'grp tod'))
for k in sorted(agg):
    a = agg[k]
    if not a[6] and not a[7] and not a[2]: continue
    print('%-7d %5d %8.0f %6d %6d %6d %7d %9d %8d' % (k, a[0], a[1] / 1024, a[2], a[3], a[4], a[5], a[6], a[7]))
    for i in range(8): tot[i] += a[i] if i != 1 else 0
    pk = max(pk, a[1])
print('total   %5d %8.0f %6d %6d %6d %7d %9d %8d' % (tot[0], pk / 1024, tot[2], tot[3], tot[4], tot[5], tot[6], tot[7]))
