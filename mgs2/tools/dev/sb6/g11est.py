#!/usr/bin/env python3
# g11est.py <gl10 log> <deck r> <inside r> [codec r]: gl11 run time [I] from gl10 run 1 windows: each part's steps at gl10 all-on mean
# (gl10 settings 0 and 2 = all on) times r (the model mean ratio over gl11 turns; 1 where not modelled)
import re, sys
def load(p):
    w = {}
    for l in open(p, errors='replace'):
        m = re.match(r'window vb (\d+): (\d+) steps, mean ([\d.]+) ms', l)
        if m: w[int(m.group(1))] = (int(m.group(2)), float(m.group(3)))
    return w
a = load(sys.argv[1]); rd, ri = float(sys.argv[2]), float(sys.argv[3]); rc = float(sys.argv[4]) if len(sys.argv) > 4 else 1.0
st = lambda W: ((W - 1001) // 100) % 2 if W > 1000 else 0   # gl10: k % 4 in (0, 2) = everything on
PARTS = [('to vb 15,600', range(100, 15601, 100), 1.0), ('call 1', range(15700, 19901, 100), rc), ('viewfinder', range(20000, 26001, 100), 1.0),
         ('call 2', range(26100, 28801, 100), rc), ('deck', list(range(28900, 31801, 100)), rd), ('inside', range(31900, 37801, 100), ri)]
t9 = t10 = 0
for name, ws, r in PARTS:
    ws = [W for W in ws if W in a]; on = [W for W in ws if st(W) == 0]
    m9 = sum(a[W][0] * a[W][1] for W in ws) / 60000
    mon = sum(a[W][1] * a[W][0] for W in on) / sum(a[W][0] for W in on)
    m10 = sum(a[W][0] for W in ws) * mon * r / 60000
    t9 += m9; t10 += m10
    print('%-12s %3d windows: gl10 %.2f min (all-on mean %.2f ms) -> gl11 %.2f min [I] (r %.3f)' % (name, len(ws), m9, mon, m10, r))
print('windows: gl10 %.2f -> gl11 %.2f min' % (t9, t10))
