# g16med.py <gl15 log> <gl16 log>: gl16's PSP run (task #148). Window line vb W = steps W-99..W in the setting of k = (W - 1001) // 100:
# gl16: k even everything on (the step 3 trims on, C1 on, C2 off), k odd the trims off (MGS2_OPT2_CYCLE ffffffdf,fffffe1f).
# gl15: k % 9 in 0, 3, 5, 7 everything on (C1 on, C2 off); the others C2 on or the GE overlap off.
# Per part: everything on, gl16 against gl15 at the vblanks where both ran everything on (means); the trims off against the mean of
# their two all-on neighbours (median, each, and the median of each part of the step); windows holding a shot left out.
import re, sys, statistics as st
def load(p):
    w = {}
    for l in open(p, errors='replace'):
        m = re.match(r'window vb (\d+): (\d+) steps, mean ([\d.]+) ms \(logic ([\d.]+), draw pipeline ([\d.]+), IOP\+sound ([\d.]+), '
                     r'GS walk ([\d.]+), translator ([\d.]+), picture ([\d.]+), GE wait ([\d.]+)\), max ([\d.]+) ms', l)
        if m: w[int(m.group(1))] = (int(m.group(2)),) + tuple(float(x) for x in m.groups()[2:])
    return w
a = load(sys.argv[1]); b = load(sys.argv[2])
def k(W): return (W - 1001) // 100 if W > 1000 else -1
def on15(W): return W > 1000 and k(W) % 9 in (0, 3, 5, 7)
def on16(W): return W > 1000 and k(W) % 2 == 0
SHOT = set(((f + 99) // 100) * 100 for f in (8760, 10516, 15761, 23263, 24244, 24784, 29500, 30500, 31112, 31663, 32501, 33613, 34911, 35027, 35589, 36941))
PARTS = [('to 15,600', 100, 15600), ('call 1', 15700, 19900), ('viewfinder', 20000, 26000), ('call 2', 26100, 28800),
         ('deck waiting', 29500, 30600), ('deck walk', 30700, 31300), ('deck other', None, None), ('inside', 31900, 37800)]
NAMES = ('logic', 'draw', 'IOP', 'GS walk', 'translator', 'picture', 'GE wait')
def wins(lo, hi):
    return list(range(28900, 29401, 100)) + list(range(31400, 31801, 100)) if lo is None else list(range(lo, hi + 1, 100))
def mean(x): return sum(x) / len(x) if x else float('nan')
print('window lines gl15 %d, gl16 %d; minutes gl15 %.2f, gl16 %.2f' % (len(a), len(b), sum(v[0] * v[1] for v in a.values()) / 60000,
      sum(v[0] * v[1] for v in b.values()) / 60000))
for name, lo, hi in PARTS:
    ws = [W for W in wins(lo, hi) if W in b]
    inr = lambda W: lo is None or (lo <= W and W <= hi)
    out = []
    on = [W for W in ws if on16(W) and on15(W) and W in a and W not in SHOT]
    if on:
        out.append('all on n%d: gl15 %.2f -> gl16 %.2f (%+.2f, %+.1f%%; GE wait %.2f -> %.2f)' % (len(on), mean([a[W][1] for W in on]),
                   mean([b[W][1] for W in on]), mean([b[W][1] - a[W][1] for W in on]),
                   100 * mean([b[W][1] - a[W][1] for W in on]) / mean([a[W][1] for W in on]), mean([a[W][8] for W in on]), mean([b[W][8] for W in on])))
    al = [W for W in ws if on16(W) and W not in SHOT]
    if al: out.append('gl16 all on n%d mean %.2f' % (len(al), mean([b[W][1] for W in al])))
    ds = []; dp = [[] for _ in NAMES]
    for W in ws:
        if W > 1000 and not on16(W) and W - 100 in b and W + 100 in b and inr(W - 100) and inr(W + 100) and not ({W, W - 100, W + 100} & SHOT):
            ds.append(b[W][1] - (b[W - 100][1] + b[W + 100][1]) / 2)
            for i in range(len(NAMES)): dp[i].append(b[W][2 + i] - (b[W - 100][2 + i] + b[W + 100][2 + i]) / 2)
    if ds:
        out.append('trims off n%d med %+.2f [%s] (parts: %s)' % (len(ds), st.median(ds), ' '.join('%+.1f' % d for d in ds),
                   ', '.join('%s %+.2f' % (NAMES[i], st.median(dp[i])) for i in range(len(NAMES)))))
    print(name + ': ' + ('; '.join(out) if out else '-'))
