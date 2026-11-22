#!/usr/bin/env python3
# g12steps.py <gl11 steps.csv> <gl12 steps.csv> [from to]: step-by-step gl12 - gl11 (exact runs) by gl12's setting
# and by the step's place in its 100-vblank window (setting k%4 of (vb-1001)//100: 0/2 on, 1 copy off, 3 marking off)
import csv, sys, collections
def load(p):
    d = {}
    for r in csv.DictReader(open(p)):
        d[int(r['vb'])] = {k: float(v) for k, v in r.items() if k != 'vb'}
    return d
a, b = load(sys.argv[1]), load(sys.argv[2])
lo, hi = (int(sys.argv[3]), int(sys.argv[4])) if len(sys.argv) > 4 else (31801, 37800)
F = ['step_us', 'iop_us', 'gs_us', 'dg_us', 'gl_us', 'gw_us']
def st(vb): return ((vb - 1001) // 100) % 4
def pos(vb): return (vb - 1001) % 100       # 0 = the first step of a window (the switch happens there)
acc = collections.defaultdict(lambda: collections.defaultdict(float)); n = collections.Counter()
for vb in range(lo, hi + 1):
    if vb not in a or vb not in b: continue
    s = st(vb); p = pos(vb); band = 'p0' if p == 0 else ('p1-9' if p < 10 else ('p10-49' if p < 50 else 'p50-99'))
    for key in (('set', s), ('set', s, band), ('set', s, 'full' if b[vb]['full'] else 'notfull')):
        n[key] += 1
        for f in F:
            acc[key][f] += b[vb][f] - a[vb][f]
            acc[key][f + '_a'] += a[vb][f]
        acc[key]['logic'] += (b[vb]['step_us'] - b[vb]['iop_us'] - b[vb]['gs_us'] - b[vb]['dg_us'] - b[vb]['gl_us'] - b[vb]['gw_us']) - \
                             (a[vb]['step_us'] - a[vb]['iop_us'] - a[vb]['gs_us'] - a[vb]['dg_us'] - a[vb]['gl_us'] - a[vb]['gw_us'])
NAME = {0: 'on (k%4 0)', 1: 'copy off', 2: 'on (k%4 2)', 3: 'marking off'}
print('gl12 - gl11, ms a step (gl11 mean in brackets), vb %d-%d' % (lo, hi))
for key in sorted(acc, key=lambda k: (k[1], len(k), str(k[2:]))):
    m = n[key]
    print('%-12s %-8s n %5d  step %+6.2f (%6.2f)  logic %+5.2f  iop %+5.2f  gs %+5.2f (%5.2f)  dg %+5.2f (%5.2f)  pic %+5.2f (%5.2f)  gw %+5.2f' % (
        NAME[key[1]], key[2] if len(key) > 2 else 'all', m, acc[key]['step_us'] / m / 1e3, acc[key]['step_us_a'] / m / 1e3,
        acc[key]['logic'] / m / 1e3, acc[key]['iop_us'] / m / 1e3, acc[key]['gs_us'] / m / 1e3, acc[key]['gs_us_a'] / m / 1e3,
        acc[key]['dg_us'] / m / 1e3, acc[key]['dg_us_a'] / m / 1e3, acc[key]['gl_us'] / m / 1e3, acc[key]['gl_us_a'] / m / 1e3, acc[key]['gw_us'] / m / 1e3))
