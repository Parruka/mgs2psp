#!/usr/bin/env python3
# g14est.py <gl13 PSP log>: gl14's run time [I] from gl13 run 1's windows, g13est.py's way with gl14's nine windows: each part's steps
# at gl13's all-on mean (gl13's turns k % 4 in (0, 2)) times r per window kind: A (k % 9 in 0, 3, 5, 7) the model's all-on ratio
# gl14 / gl13 (deck 143.6 / 141.5, inside 88.8 / 87.8, codec 81.2 / 86.2); B T1 off: A + T1's saving (sb10 model without PGO: deck 2.02,
# inside 1.18, codec 2.45 ms); D T2 off: A + T2's (deck 0.33, inside 0.25, codec 4.45); C C1 on: codec 79.6 / 86.2, else A; E and F
# GE overlap off: A (F: C1's codec ratio) + the GE's own frame time a step, gl4 run 2's indoor +14.22 ms a step [C, psp_harness.md
# "Build gl4"] taken for every part, scaled by the part's all-on mean over gl4's 126.41 [I]; parts with no model: r 1
import re, sys
def load(p):
    w = {}
    for l in open(p, errors='replace'):
        m = re.match(r'window vb (\d+): (\d+) steps, mean ([\d.]+) ms', l)
        if m: w[int(m.group(1))] = (int(m.group(2)), float(m.group(3)))
    return w
a = load(sys.argv[1])
on13 = lambda W: W <= 1000 or ((W - 1001) // 100) % 2 == 0
s9 = lambda W: ((W - 1001) // 100) % 9 if W > 1000 else 0
M13 = {'deck': 141.5, 'inside': 87.8, 'codec': 86.2}; M14 = {'deck': 143.6, 'inside': 88.8, 'codec': 81.2}
T1 = {'deck': 2.02, 'inside': 1.18, 'codec': 2.45}; T2 = {'deck': 0.33, 'inside': 0.25, 'codec': 4.45}; C1 = {'codec': 79.6}
PARTS = [('to vb 15,600', range(100, 15601, 100), None), ('call 1', range(15700, 19901, 100), 'codec'), ('viewfinder', range(20000, 26001, 100), None),
         ('call 2', range(26100, 28801, 100), 'codec'), ('deck', range(28900, 31801, 100), 'deck'), ('inside', range(31900, 37801, 100), 'inside')]
t13 = t14 = 0
for name, ws, k in PARTS:
    ws = [W for W in ws if W in a]; on = [W for W in ws if on13(W)]
    m13 = sum(a[W][0] * a[W][1] for W in ws) / 60000
    mon = sum(a[W][1] * a[W][0] for W in on) / sum(a[W][0] for W in on)
    ge = 14.22 * mon / 126.41
    def ms(W):
        s = s9(W)
        if k is None: return mon + (ge if s in (1, 2) else 0)
        base = M14[k]; sc = mon / M13[k]   # PSP ms per model ms
        if s == 4: base += T1[k]
        elif s == 6: base += T2[k]
        elif s in (8, 2) and k in C1: base = C1[k]
        return base * sc + (ge if s in (1, 2) else 0)
    m14 = sum(a[W][0] * ms(W) for W in ws) / 60000
    t13 += m13; t14 += m14
    print('%-12s %3d windows: gl13 %.2f min (all-on %.2f ms) -> gl14 %.2f min [I]' % (name, len(ws), m13, mon, m14))
print('windows: gl13 %.2f -> gl14 %.2f min [I]' % (t13, t14))
