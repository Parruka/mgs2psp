#!/usr/bin/env python3
"""parts17.py <gl17 log>: gl17's effects-off windows (k even), shot windows left out: the mean of each part a step, by area
(g17med.py's areas), for the roadmap's "Where we are" table."""
import re, sys
w = {}
for l in open(sys.argv[1], errors='replace'):
    m = re.match(r'window vb (\d+): (\d+) steps, mean ([\d.]+) ms \(logic ([\d.]+), draw pipeline ([\d.]+), IOP\+sound ([\d.]+), '
                 r'GS walk ([\d.]+), translator ([\d.]+), picture ([\d.]+), GE wait ([\d.]+)\), max ([\d.]+) ms', l)
    if m: w[int(m.group(1))] = (int(m.group(2)),) + tuple(float(x) for x in m.groups()[2:])
def k(W): return (W - 1001) // 100 if W > 1000 else -1
SHOT = set(((f + 99) // 100) * 100 for f in (8760, 10516, 15761, 23263, 24244, 24784, 29500, 30500, 31112, 31663, 32501, 33613, 34911, 35027, 35589, 36941))
PARTS = [('to 15,600', 100, 15600), ('call 1', 15700, 19900), ('viewfinder', 20000, 26000), ('call 2', 26100, 28800),
         ('deck waiting', 29500, 30600), ('deck walk', 30700, 31300), ('deck other', None, None), ('inside', 31900, 37800)]
def wins(lo, hi):
    return list(range(28900, 29401, 100)) + list(range(31400, 31801, 100)) if lo is None else list(range(lo, hi + 1, 100))
print('area: n, step, logic, draw pipeline, IOP+sound, GS walk+translator, picture, GE wait (ms a step, means of the effects-off windows)')
for name, lo, hi in PARTS:
    ws = [W for W in wins(lo, hi) if W in w and W > 1000 and k(W) % 2 == 0 and W not in SHOT]
    if not ws: print(name, 'none'); continue
    s = [sum(w[W][i] for W in ws) / len(ws) for i in range(1, 10)]
    print('%-13s n%-3d %7.2f  %6.2f %6.2f %5.2f %6.2f %6.2f %5.2f   (fps %.2f)' % (name, len(ws), s[0], s[1], s[2], s[3], s[4] + s[5], s[6], s[7], 500 / s[0]))
