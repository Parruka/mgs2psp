# gbstat.py LOG...: frame arena data per picture by stretch (the 'xlive gb: vb N: ... K KB data' lines): max / mean KB, the vb of the max,
# batches max, dropped pictures
import gzip, re, sys
ST = [(0, 24000, 'opening <24,000'), (24000, 29490, 'deck 24,000-29,489'), (29490, 31401, 'rain 29,490-31,400'), (31401, 10**9, 'after 31,401')]
rx = re.compile(r'^xlive gb: vb (\d+): (\d+) batches \(\d+ transform\), (\d+) vertices, (\d+) KB data.*dropped (\d+)')
for f in sys.argv[1:]:
    d = {s[2]: [] for s in ST}
    for l in gzip.open(f, 'rt', errors='replace'):
        m = rx.match(l)
        if not m: continue
        vb, nb, nv, kb, dr = map(int, m.groups())
        for a, b, n in ST:
            if a <= vb < b: d[n].append((kb, vb, nb, nv, dr))
    print(f.split('/')[-1])
    for a, b, n in ST:
        x = d[n]
        if not x: continue
        mx = max(x); print('  %-20s pics %5d data max %4d KB (vb %d) mean %4d KB; batches max %d; vertices max %d; pictures with drops %d' % (
            n, len(x), mx[0], mx[1], sum(t[0] for t in x) / len(x), max(t[2] for t in x), max(t[3] for t in x), sum(1 for t in x if t[4])))
