# kb.py log.gz [log.gz ...]: the frame arena's data per picture ('xlive gb: vb N: ... K KB data'), max and mean by stretch, and the
# pictures with 'no room' calls
import sys, gzip, re
R = [(0, 24000, 'opening <24000'), (24000, 29490, 'deck 24000-29489'), (29490, 31401, 'rain 29490-31400'), (31401, 99999, 'after 31401')]
for p in sys.argv[1:]:
    s = {r[2]: [0, 0, 0, 0] for r in R}
    for l in gzip.open(p, 'rt', errors='replace'):
        if not l.startswith('xlive gb: vb '): continue
        m = re.match(r'xlive gb: vb (\d+): .*? (\d+) KB data,.*?no room (\d+)', l)
        if not m: continue
        v, kb, nr = int(m.group(1)), int(m.group(2)), int(m.group(3))
        for a, b, n in R:
            if a <= v < b: x = s[n]; x[0] += 1; x[1] = max(x[1], kb); x[2] += kb; x[3] += nr > 0
    print(p); [print('  %-18s pictures %5d  max %5d KB  mean %5.0f KB  pictures with no-room calls %d' % (n, x[0], x[1], x[2] / max(x[0], 1), x[3])) for n, x in s.items()]
