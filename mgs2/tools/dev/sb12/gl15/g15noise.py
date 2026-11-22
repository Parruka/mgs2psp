# g15noise.py <gl15 log>: a placebo for g15med.py: each all-on window k%9 = 5 against the mean of the all-on windows 200 vb away (k%9 3, 7),
# step and picture part, per part (medians), as the C2 turns are read against their neighbours 100 vb away
import re, sys, statistics as st
w = {}
for l in open(sys.argv[1], errors='replace'):
    m = re.match(r'window vb (\d+): (\d+) steps, mean ([\d.]+) ms \(logic ([\d.]+), draw pipeline ([\d.]+), IOP\+sound ([\d.]+), GS walk ([\d.]+), translator ([\d.]+), picture ([\d.]+), GE wait ([\d.]+)\), max ([\d.]+) ms', l)
    if m: w[int(m.group(1))] = (int(m.group(2)),) + tuple(float(x) for x in m.groups()[2:])
s9 = lambda W: ((W - 1001) // 100) % 9
SHOT = set(((f + 99) // 100) * 100 for f in (8760, 10516, 15761, 23263, 24244, 24784, 29500, 30500, 31112, 31663, 32501, 33613, 34911, 35027, 35589, 36941))
for name, lo, hi in [('to 15,600', 100, 15600), ('call 1', 15700, 19900), ('viewfinder', 20000, 26000), ('call 2', 26100, 28800), ('deck', 28900, 31800), ('inside', 31900, 37800)]:
    d = []; p = []
    for W in range(lo, hi + 1, 100):
        if W > 1000 and W in w and s9(W) == 5 and W - 200 in w and W + 200 in w and lo <= W - 200 and W + 200 <= hi and not ({W, W - 200, W + 200} & SHOT):
            d.append(w[W][1] - (w[W - 200][1] + w[W + 200][1]) / 2); p.append(w[W][7] - (w[W - 200][7] + w[W + 200][7]) / 2)
    if d: print('%s: placebo n%d step med %+.2f [%s]; picture med %+.2f' % (name, len(d), st.median(d), ' '.join('%+.1f' % x for x in d), st.median(p)))
